// W3C "standard" Gamepad mapping -> the engine's native rage::ioPad layout.
import {mappedInputs, deviceKey, controllerStyle, createProfileStore} from './controller-profiles.js';
import {installControllerSetup} from './controller-setup.js';
import {createInputLoop} from './controller-loop.js';
// Keep stick magnitude intact: the game applies its own 20% analog dead zone.
// Engine enum: RUP=4, RRIGHT=5, RDOWN=6, RLEFT=7, SELECT=8, START=11.
export const BUTTON_BITS = [64, 32, 128, 16, 4, 8, 1, 2, 256, 2048, 512, 1024, 4096, 16384, 32768, 8192];
const clamp = (value, min, max) => Math.max(min, Math.min(max, Number.isFinite(value) ? value : 0));
export function sampleGamepad(pad, profile = null) {
  const {values, axes} = profile ? mappedInputs(pad,profile) : {
    values: Array.from({length: 17}, (_, i) => clamp(pad.buttons[i]?.value ?? (pad.buttons[i]?.pressed ? 1 : 0), 0, 1)),
    axes: Array.from({length: 4}, (_, i) => clamp(pad.axes[i], -1, 1))};
  let mask = 0;
  for (let i = 0; i < BUTTON_BITS.length; i++) if (values[i] > (i === 6 || i === 7 ? .12 : .5)) mask |= BUTTON_BITS[i];
  return {mask, axes, values, bytes: axes.map(v => Math.round((v + 1) * 127.5)), triggers: [6, 7].map(i => Math.round(values[i] * 255))};
}
export function writeControllerState(state, sample, {connected, active, focused, now = Date.now()}) {
  Atomics.add(state, 0, 1);
  Atomics.store(state, 1, connected ? 1 : 0);
  Atomics.store(state, 2, active ? 1 : 0);
  Atomics.store(state, 3, focused ? 1 : 0);
  Atomics.store(state, 4, sample?.mask || 0);
  for (let i = 0; i < 4; i++) Atomics.store(state, 5 + i, sample?.bytes[i] ?? 128);
  Atomics.store(state, 9, sample?.triggers[0] || 0);
  Atomics.store(state, 10, sample?.triggers[1] || 0);
  Atomics.store(state, 11, now | 0);
  Atomics.add(state, 0, 1);
}
export function createControllerTracker(resolve = pad => pad.mapping === 'standard' ? {profile:null} : null) {
  let selected = null, mode = 'keyboard', previous = new Map(), armed = true, waitForNeutral = false;
  const key = p => p.index+':'+deviceKey(p);
  return {
    select(pad) { selected=key(pad); mode='controller'; armed=true; },
    keyboard(reset = false) { mode = 'keyboard'; armed = false; if (reset) waitForNeutral=true; },
    poll(pads, focused, blocked = false) {
      const available = [...pads].filter(p => p?.connected);
      const layouts = new Map(available.map(p => [p,resolve(p)]));
      const supported = available.filter(p => layouts.get(p));
      const samples = new Map(supported.map(p => [key(p),sampleGamepad(p,layouts.get(p).profile)]));
      const buttonDown = (s,i) => (s?.values[i] || 0) > (i===6||i===7 ? .25 : .5);
      const significant = s => s.values.some((_,i) => buttonDown(s,i)) || s.axes.some(v => Math.abs(v) > .25);
      if (waitForNeutral) {
        blocked=true;
        if ([...samples.values()].every(s => !significant(s))) waitForNeutral=false;
      }
      let pad = supported.find(p => selected && key(p) === selected);
      const changed = supported.find(p => {
        const s = samples.get(key(p)), old = previous.get(key(p));
        const pressed = s.values.some((_,i) => buttonDown(s,i) && !buttonDown(old,i));
        const moved = s.axes.some((v,i) => Math.abs(v) > .25 &&
          (Math.abs(old?.axes[i] || 0) <= .25 || (armed && Math.abs(v - old.axes[i]) > .12)));
        return pressed || moved;
      });
      if (!pad) { pad = supported[0]; if (pad && focused && !blocked) { mode = 'controller'; armed = true; } }
      // A newly exposed physical pad replaces idle touch controls automatically.
      const plugged = supported.find(p => !p.touch && !previous.has(key(p)));
      if (pad?.touch && plugged && focused && !blocked) { pad=plugged; mode='controller'; armed=true; }
      if (focused && !blocked && changed) {
        pad = changed; mode = 'controller'; armed = true;
      }
      if (!pad || !focused || blocked) { mode = 'keyboard'; armed = false; }
      const sample = pad ? samples.get(key(pad)) : null;
      if (!sample || !significant(sample)) armed = true;
      const old = pad && previous.get(key(pad));
      const presses = sample ? sample.values.map((v,i) => v > .5 && !(old?.values[i] > .5)) : [];
      selected = pad ? key(pad) : null;
      previous = samples;
      return {pad, sample, presses, mode, connected: !!pad, active: mode === 'controller', focused,
        style: pad ? layouts.get(pad).profile?.style || controllerStyle(pad) : 'generic',
        unsupported: available.some(p => !supported.includes(p))};
    }
  };
}
export function installController({onMode = () => {}, onMenu = () => false, menuOpen = () => false, onActivate = () => {}, touch = null} = {}) {
  let storage; try { storage=localStorage; } catch { /* Private browser policy. */ }
  const profiles=createProfileStore(storage);
  const layouts = new Map();
  const tracker=createControllerTracker(pad => {
    const old = layouts.get(pad.index);
    if (old && old.id === pad.id && old.mapping === pad.mapping && old.buttons === pad.buttons.length && old.axes === pad.axes.length) return old.layout;
    const profile=profiles.get(pad);
    const layout = profile || pad.mapping==='standard' ? {profile} : null;
    layouts.set(pad.index, {id:pad.id, mapping:pad.mapping, buttons:pad.buttons.length, axes:pad.axes.length, layout});
    return layout;
  });
  const buffer = new SharedArrayBuffer(48), state = new Int32Array(buffer);
  let current = {connected:false, active:false, focused:false}, lastLabel = '', lastMode = '', ignoreMouseUntil = 0, lastSetup = -Infinity;
  const help = document.createElement('details');
  help.id = 'controller-help';
  help.innerHTML = '<summary id="controller-status">Controller: press a button to connect</summary><div><p>Connect by USB or Bluetooth, then press a button with this page focused. Xbox, PlayStation and standard gamepads connect automatically. Older USB adapters can be set up below.</p><p id="controller-reference"></p><p>Use the keyboard or mouse to switch back. Release a held stick before using it to switch to the controller again.</p><p>The game may show Xbox artwork: A=✕, B=○, X=□, Y=△. Click Enable sound once if audio is blocked.</p></div>';
  const style = document.createElement('style');
  style.textContent = '#controller-help{position:fixed;right:12px;top:52px;z-index:20;color:#eee;font:12px/1.5 system-ui,sans-serif;max-width:min(390px,90vw);max-height:calc(100vh - 70px);overflow:auto;background:rgba(20,20,24,.94);border:1px solid #666;border-radius:6px}#controller-help summary{cursor:pointer;padding:7px 10px}#controller-help>div{padding:4px 12px 12px}#controller-help p{margin:7px 0}#controller-help select{max-width:100%;width:100%}#controller-help button,#controller-help select,#controller-setup button,#controller-setup select{font:inherit;color:#fff;background:#303039;border:1px solid #888;border-radius:4px;padding:6px;cursor:pointer}#controller-help button:disabled,#controller-setup button:disabled{opacity:.45;cursor:default}#controller-help hr{border:0;border-top:1px solid #555}#controller-reference{white-space:pre-line}#controller-setup{color:#eee;background:#191920;border:1px solid #999;border-radius:10px;padding:24px;width:min(480px,80vw);max-height:85vh;overflow:auto;font:14px/1.5 system-ui,sans-serif}#controller-setup::backdrop{background:#000b}#controller-setup h2{margin-top:0}#setup-device{overflow-wrap:anywhere;color:#bbb;font-size:12px}#setup-progress{color:#aaa}#setup-review{font-size:12px}#controller-setup .setup-actions{margin-top:20px;display:flex;gap:8px;flex-wrap:wrap}#controller-setup [hidden]{display:none}#controller-setup #setup-save{background:#245d41}';
  document.head.append(style); document.body.append(help);
  const status = help.querySelector('summary');
  const reference = help.querySelector('#controller-reference');
  const focused = () => !document.hidden && document.hasFocus();
  const inUI = e => !!e?.target?.closest?.('#controller-help, #controller-setup, #offline-audio, #touch-controls, #touch-toggle, #modes');
  const fromTouch = e => e?.pointerType === 'touch' || e?.sourceCapabilities?.firesTouchEvents;
  touch?.setOnActivate(() => { tracker.select(touch.pad); });
  const setup=installControllerSetup(help,profiles,() => {
    layouts.clear(); lastSetup = -Infinity;
    touch?.clear(); onActivate(); tracker.keyboard(true); current.active=false; current.mode='keyboard';
    ignoreMouseUntil=performance.now()+180; publish();
  });
  function publish() {
    writeControllerState(state, current.sample, {...current, focused: focused(), active: current.active && !setup.blocked && !menuOpen()});
  }
  function keyboard(e) {
    if (inUI(e) || fromTouch(e) || setup.blocked) return;
    touch?.clear();
    tracker.keyboard(); current.active = false; current.mode='keyboard'; updateMode(); publish();
  }
  addEventListener('keydown', e => { if (!e.repeat) keyboard(e); }, {capture:true});
  addEventListener('pointerdown', keyboard, {capture:true});
  addEventListener('mousemove', e => { if (performance.now() >= ignoreMouseUntil && Math.abs(e.movementX) + Math.abs(e.movementY) > 2) keyboard(e); }, {capture:true});
  addEventListener('wheel', keyboard, {capture:true, passive:true});
  addEventListener('blur', () => { tracker.keyboard(); current.active=false; publish(); });
  function suspend() {
    inputLoop.stop(); touch?.clear(); tracker.keyboard(true);
    current = {connected:false,active:false,focused:false}; publish();
  }
  document.addEventListener('visibilitychange', () => { if (document.hidden) suspend(); else inputLoop.start(); });
  addEventListener('pagehide', suspend);
  addEventListener('pageshow', () => { if (!document.hidden) inputLoop.start(); });
  for (const event of ['gamepadconnected','gamepaddisconnected']) addEventListener(event, () => { layouts.clear(); lastSetup = -Infinity; });
  function updateMode() {
    const playstation = current.style==='playstation';
    const modeKey = current.mode + ':' + current.style + ':' + !!current.pad?.touch;
    if (modeKey !== lastMode) {
      if (current.active) { onActivate(); ignoreMouseUntil=performance.now()+180; }
      onMode(current.active, playstation); lastMode = modeKey;
      reference.textContent='Left stick: move / steer · Right stick: camera\n'+(playstation
        ?'L2 / R2: aim / fire, brake / accelerate\n✕: sprint / confirm · ○: cancel / reload\n□: jump · △: enter / exit vehicle\nL1: weapon wheel · R1: cover / handbrake\nL3 / R3: crouch / look behind\nOptions: pause · Select / Share: change view'
        :'LT / RT: aim / fire, brake / accelerate\nA: sprint / confirm · B: cancel / reload\nX: jump · Y: enter / exit vehicle\nLB: weapon wheel · RB: cover / handbrake\nStick clicks: crouch / look behind\nStart / Menu: pause · Back / View: change view')+'\nD-pad: normal directional actions';
    }
  }
  function tick(now) {
    let pads = [], error = false;
    try { pads = [...(navigator.getGamepads?.() || [])]; } catch { error = true; }
    if (setup.blocked || now - lastSetup >= 250) { setup.tick(pads,focused(),now); lastSetup = now; }
    for (const index of layouts.keys()) if (!pads.some(p=>p?.connected && p.index===index) && index!==touch?.pad.index) layouts.delete(index);
    if (touch?.pad.connected) pads.push(touch.pad);
    current = tracker.poll(pads, focused(), setup.blocked);
    touch?.update({physicalActive:current.active && !current.pad?.touch, menuOpen:menuOpen(), setupOpen:setup.blocked});
    updateMode();
    const brand={playstation:'PlayStation',xbox:'Xbox',generic:'Gamepad'}[current.style];
    const label = current.pad?.touch ? (current.active ? 'Touch controls' : 'Keyboard / mouse') : error ? 'Controller access unavailable' : current.connected
      ? (current.active ? 'Controller mode' : 'Keyboard / mouse') + ' · ' + brand
      : current.unsupported ? 'Controller connected · set up layout' : 'Controller: press a button to connect';
    if (label !== lastLabel) { status.textContent = label; status.title = current.pad?.id || ''; lastLabel = label; }
    if (!setup.blocked && current.connected && current.active && current.focused && menuOpen()) onMenu(current.presses);
    publish();
  }
  const inputLoop = createInputLoop(tick);
  if (!document.hidden) inputLoop.start();
  // Polling also detects devices connected before the page opened.
  return {buffer, keyboard,
    acceptsKeyboard: e => !setup.blocked && !inUI(e) && !(current.active && e.repeat),
    acceptsMouse: e => !setup.blocked && !inUI(e) && !fromTouch(e) && !current.active};
}
