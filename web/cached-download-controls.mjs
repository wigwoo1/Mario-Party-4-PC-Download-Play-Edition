// Removing a download is a separate, explicit action after stopping. Runtime
// cleanup must settle before deleting the selected game's browser data.
export function cachedDownloadControls(button,info,{available,settled,epoch,openStore,status}){
  let selected,busy=false;
  const refresh=()=>{
    const disabled=busy||!selected||!available();if(button.disabled!==disabled)button.disabled=disabled;
    const text=selected?`Downloaded game: ${selected.name}. Stop the session before removing it.`:'No downloaded game selected.';
    if(info.textContent!==text)info.textContent=text;
  };
  button.onclick=async()=>{
    if(busy||!selected||!available())return;
    const chosen=selected,originalEpoch=epoch();let storage;
    busy=true;refresh();
    try{
      await settled();
      if(epoch()!==originalEpoch||!available())return;
      storage=await openStore();
      if(epoch()!==originalEpoch||!available())return;
      await storage.remove(chosen.id);
      if(selected===chosen)selected=undefined;
      if(epoch()===originalEpoch)status('Download removed from this browser. Other downloaded games are kept.');
    }catch(error){if(epoch()===originalEpoch)status('Could not remove download: '+error.message);}
    finally{storage?.close();busy=false;refresh();}
  };
  refresh();
  return {refresh,remember(manifest){
    if(typeof manifest?.id!=='string'||!/^[a-f0-9]{64}$/.test(manifest.id))throw Error('Valid offered game required.');
    selected={id:manifest.id,name:String(manifest.name||'Game').slice(0,200)};refresh();
  }};
}
