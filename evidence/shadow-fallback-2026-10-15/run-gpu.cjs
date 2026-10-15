const fs=require('node:fs'),p=require('node:path'),http=require('node:http'),{spawn}=require('node:child_process');
const browser='C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe';
const profile=p.join(__dirname,'browser-profile-'+Date.now());
let child,connection,server;
if(!fs.existsSync(p.join(__dirname,'fixture-shader.json')))throw Error('Run prepare-fixture.cjs successfully before the GPU test');
async function waitUntil(fn,ms=20000){const deadline=Date.now()+ms;while(Date.now()<deadline){const v=await fn();if(v)return v;await new Promise(r=>setTimeout(r,100));}throw Error('Browser test setup timed out');}
(async()=>{
 server=http.createServer((req,res)=>{
  const files={'/gpu-fixture.js':['gpu-fixture.js','text/javascript'],'/fixture-shader.json':['fixture-shader.json','application/json']};
  if(req.url==='/'){res.setHeader('Content-Type','text/html');res.end('<!doctype html><title>Native shadow compatibility fixture</title><body>Running<script src="/gpu-fixture.js"></script>');return;}
  const file=files[req.url];if(!file){res.writeHead(404);res.end();return;}res.setHeader('Content-Type',file[1]);res.end(fs.readFileSync(p.join(__dirname,file[0])));
 });
 await new Promise(r=>server.listen(0,'127.0.0.1',r));const port=server.address().port;
 const log=fs.openSync(p.join(__dirname,'browser-test.log'),'w');
 child=spawn(browser,['--headless=new','--no-first-run','--no-default-browser-check','--disable-extensions','--disable-background-networking','--remote-debugging-port=0','--user-data-dir='+profile,'http://127.0.0.1:'+port],{windowsHide:true,stdio:['ignore',log,log]});
 fs.closeSync(log);child.on('error',e=>console.error(e.message));
 const debug=await waitUntil(()=>{const f=p.join(profile,'DevToolsActivePort');return fs.existsSync(f)?fs.readFileSync(f,'utf8').split('\n')[0]:null;});
 const target=await waitUntil(async()=>{const r=await fetch('http://127.0.0.1:'+debug+'/json/list');return (await r.json()).find(x=>x.type==='page'&&x.url.includes('127.0.0.1:'+port));});
 connection=new WebSocket(target.webSocketDebuggerUrl);await new Promise((resolve,reject)=>{connection.addEventListener('open',resolve,{once:true});connection.addEventListener('error',reject,{once:true});});
 let next=1;const pending=new Map();connection.addEventListener('message',event=>{const r=JSON.parse(event.data);if(r.id&&pending.has(r.id)){const slot=pending.get(r.id);pending.delete(r.id);r.error?slot.reject(Error(JSON.stringify(r.error))):slot.resolve(r.result);}});
 const call=(method,params={})=>new Promise((resolve,reject)=>{const id=next++;pending.set(id,{resolve,reject});connection.send(JSON.stringify({id,method,params}));});
 await waitUntil(async()=>{const r=await call('Runtime.evaluate',{expression:'typeof window.runDone !== "undefined"',returnByValue:true});return r.result.value;});
 const response=await call('Runtime.evaluate',{expression:'window.runDone',awaitPromise:true,returnByValue:true,timeout:120000});
 if(response.exceptionDetails)throw Error(JSON.stringify(response.exceptionDetails));
 const result=response.result.value;
 fs.writeFileSync(p.join(__dirname,'gpu-result.json'),JSON.stringify(result,null,2));console.log(JSON.stringify(result));
 await call('Browser.close');connection.close();connection=null;
 if(result.result!=='PASS')process.exitCode=1;
})().catch(e=>{console.error(e.stack);process.exitCode=1;}).finally(()=>{if(connection)connection.close();if(child&&!child.killed)child.kill();if(server)server.close();});
