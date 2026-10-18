const test=require('node:test'),assert=require('node:assert/strict');
const {createFullMatchProgress}=require('./full-match-progress.cjs');
const options={board:0,maxTurn:20,awardOverlay:73};
const sample=(turn,overlay=89,event=0)=>({board:0,maxTurn:20,turn,overlay,event});
test('all normal turns followed by awards and rankings are required, with rendering still unverified',()=>{
 const gate=createFullMatchProgress(options);for(let turn=1;turn<=20;turn++)assert.equal(gate.observe(sample(turn)),false);
 assert.equal(gate.observe(sample(20,73,0)),false);assert.equal(gate.observe(sample(20,73,1)),true);
 assert.equal(gate.snapshot().renderedRankingsVerified,false);assert.equal(gate.snapshot().observedTurns.length,20);
});
test('last turn, repeated cycles or rankings without observed awards cannot prove a complete match',()=>{
 for(const mode of ['last','cycle','rankings']){
  const gate=createFullMatchProgress(options);
  if(mode==='last'){gate.observe(sample(20,73,0));assert.equal(gate.observe(sample(20,73,1)),false);}
  if(mode==='cycle'){for(let repeat=0;repeat<20;repeat++)gate.observe(sample(1));assert.equal(gate.snapshot().completeProgress,false);}
  if(mode==='rankings'){for(let turn=1;turn<=20;turn++)gate.observe(sample(turn));assert.equal(gate.observe(sample(20,73,1)),false);}
 }
});
test('a five-turn game reaches completion only after its fifth-turn awards and rankings',()=>{
 const gate=createFullMatchProgress({board:0,maxTurn:5,awardOverlay:73});
 for(let turn=1;turn<=5;turn++)assert.equal(gate.observe({board:0,maxTurn:5,turn,overlay:89,event:0}),false);
 assert.equal(gate.observe({board:0,maxTurn:5,turn:5,overlay:73,event:0}),false);
 assert.equal(gate.observe({board:0,maxTurn:5,turn:5,overlay:73,event:1}),true);
 assert.deepEqual(gate.snapshot().observedTurns,[1,2,3,4,5]);
});
test('only configured party lengths from 5 through 50 are accepted',()=>{
 for(const maxTurn of [0,4,21,55])assert.throws(()=>createFullMatchProgress({board:0,maxTurn,awardOverlay:73}));
});
test('changed turn count/board, missing event getter and backwards progress reject the gate',()=>{
 for(const changed of [{maxTurn:10},{board:1},{event:-1},{turn:0}]){
  const gate=createFullMatchProgress(options);gate.observe(sample(1));assert.throws(()=>gate.observe({...sample(2),...changed}));
 }
 const gate=createFullMatchProgress(options);gate.observe(sample(3));assert.throws(()=>gate.observe(sample(2)));
});
