// The adapter reads actual asset/configuration/shader receipts. No elapsed-time
// shortcut is permitted to mark a device ready.
export function automaticPreparation({state,accept,prepare,ready,error}){
 let busy=false,closed=false,failed=false,accepted=false,prepared=false,readyMembership;
 return {
  async refresh(){
   if(busy||closed||failed)return;busy=true;
   try{
    let current=state();if(!current||current.running)return;
    if(current.canAccept&&!accepted){accepted=true;await accept();if(closed)return;current=state();}
    if(!current?.assetsVerified||!current.configuration||!current.seedVerified)return;
    if(!current.prepared&&!prepared){prepared=true;await prepare();if(closed)return;current=state();if(!current?.prepared)throw Error('Game preparation did not produce a verified runtime.');}
    if(current?.prepared&&current.assignedVerified&&!current.ready&&current.membership!==readyMembership){
     await ready();if(!closed)readyMembership=current.membership;
    }
   }catch(cause){if(!closed){failed=true;error(cause);}}finally{busy=false;}
  },
  close(){closed=true;},
 };
}
