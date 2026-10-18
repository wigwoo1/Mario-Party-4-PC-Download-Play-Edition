'use strict';
// Drive ordinary Host keyboard controls through drive-extra-room's owned test
// mailbox. Never change game memory, seed, score, overlay or peer input records.
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const output=process.argv[2];assert.ok(output);const directory=path.join(output,'extra-room'),sleep=ms=>new Promise(resolve=>setTimeout(resolve,ms));
const read=name=>JSON.parse(fs.readFileSync(path.join(directory,name),'utf8'));
const deadline=Date.now()+1200000;
async function until(check){while(Date.now()<deadline){for(const name of ['failure.json','../failure.json'])if(fs.existsSync(path.join(directory,name)))throw Error(fs.readFileSync(path.join(directory,name),'utf8').slice(0,2000));if(await check())return;await sleep(100);}throw Error('Medic replay controller budget exceeded; inspect the live driver');}
async function command(id,key,frames=210){fs.writeFileSync(path.join(directory,'command.json'),JSON.stringify({id,action:'press',key,frames,hold:80}));await until(()=>fs.existsSync(path.join(directory,'command-complete.json'))&&read('command-complete.json').id===id);const result=read('command-complete.json').result;console.log(JSON.stringify({id,frame:result.after.game.frame,overlay:result.after.game.progress.overlay}));return result;}
async function round(prefix){await command(prefix+'-choose','z',300);const start=await command(prefix+'-active','Enter',720);assert.equal(start.after.game.progress.overlay,65);await command(prefix+'-patient','z',1200);await until(()=>read('latest.json').game.progress.overlay===75);await command(prefix+'-score','z');await command(prefix+'-ranking','z',240);const returned=await command(prefix+'-return','z',480);assert.equal(returned.after.game.progress.overlay,75);}
(async()=>{
 await until(()=>fs.existsSync(path.join(directory,'extra-open-browser.png'))&&fs.existsSync(path.join(directory,'latest.json'))&&read('latest.json').game.progress.overlay===75);
 for(const id of ['welcome','skip-info','room-select'])await command(id,'z');
 await command('whomp-right','ArrowRight');await command('whomp-open','z',600);await command('mario','z',300);await command('game-list','z',300);await command('medic-down1','ArrowDown',180);await command('medic-down2','ArrowDown',180);
 await round('first');await round('replay');
 await command('returned-up','ArrowUp',210);await command('returned-down','ArrowDown',210);
 fs.writeFileSync(path.join(directory,'controller-execution.json'),JSON.stringify({status:'execution-passed-visual-review-required',normalKeyboardOnly:true,rounds:2,requiredImages:['first-active','first-patient','first-score','first-ranking','first-return','replay-active','replay-patient','replay-score','replay-ranking','replay-return','returned-up','returned-down'],automaticallyFinished:false,at:new Date().toISOString()},null,2)+'\n',{flag:'wx'});
 console.log('Normal-menu Medic replay complete; inspect paired captures before sending finish.');
})().catch(error=>{if(fs.existsSync(directory))fs.writeFileSync(path.join(directory,'controller-failure.json'),JSON.stringify({error:error.stack},null,2));console.error(error.message);process.exitCode=1;});
