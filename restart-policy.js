export function shouldPauseRestart(previous, navigationType, now=Date.now()) {
  return !!previous && Number.isFinite(previous.updated) && now>=previous.updated &&
    now-previous.updated<30*60*1000 && (previous.active===true || navigationType==='reload');
}
