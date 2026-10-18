'use strict';
const test=require('node:test'),assert=require('node:assert/strict');
const {rankingCaptureFrameReached,rankingCaptureTargetFrame}=require('./drive-production-full-match.cjs');

test('ranking capture waits for fresh foreground frames and rejects a changed or ended match',()=>{
 let state={input:{state:'Running',frame:'900'},progress:{overlay:73,event:1}};
 global.partyBoardApp={lobbyGameState:()=>state};
 try{
  const args={frame:rankingCaptureTargetFrame(state.input.frame),overlay:73};
  assert.equal(args.frame,'960');
  assert.equal(rankingCaptureFrameReached(args),false,'The pre-wait frame cannot satisfy its own capture gate.');
  state={...state,input:{...state.input,frame:'959'}};
  assert.equal(rankingCaptureFrameReached(args),false,'The browser must produce all requested fresh frames.');
  state={...state,input:{...state.input,frame:'960'}};
  assert.equal(rankingCaptureFrameReached(args),true);
  state={...state,progress:{...state.progress,event:0}};
  assert.equal(rankingCaptureFrameReached(args),false,'A departure from rankings must cancel the capture condition.');
  state={...state,input:{...state.input,state:'Stopped'},progress:{...state.progress,event:1}};
  assert.equal(rankingCaptureFrameReached(args),false,'A stopped peer must not pass the rankings gate.');
 }finally{delete global.partyBoardApp;}
});

test('ranking capture frame arithmetic supports large emulated frame counters',()=>{
 assert.equal(rankingCaptureTargetFrame('900719925474099312345',60),'900719925474099312405');
 for(const frame of ['', '-1', '1.5', 'x'])assert.throws(()=>rankingCaptureTargetFrame(frame));
 for(const advance of [0,-1,1.5,Number.MAX_SAFE_INTEGER+1])assert.throws(()=>rankingCaptureTargetFrame('1',advance));
});
