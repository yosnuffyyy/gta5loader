// One animation callback, bounded to 60 input samples/s even on a 120 Hz screen.
// No worker, interval or catch-up burst. Lifecycle events explicitly restart it.
export function createInputLoop(sample, {request = requestAnimationFrame, cancel = cancelAnimationFrame} = {}) {
  const interval = 1000 / 60;
  let running = false, handle = 0, next = -Infinity;
  function frame(now) {
    handle = 0;
    if (!running) return;
    if (now + 0.01 >= next) {
      next = Number.isFinite(next) ? next + (Math.floor((now - next + 0.01) / interval) + 1) * interval : now + interval;
      sample(now);
    }
    if (running) handle = request(frame);
  }
  return {
    start() { if (running) return; running = true; next = -Infinity; handle = request(frame); },
    stop() { running = false; if (handle) cancel(handle); handle = 0; next = -Infinity; }
  };
}
