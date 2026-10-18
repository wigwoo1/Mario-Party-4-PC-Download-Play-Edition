// Asset-free rendering component. Caller supplies validated host snapshots.
export function renderLobby(container,snapshot,localPeer) {
  const doc=container.ownerDocument, fragment=doc.createDocumentFragment();
  for(let slot=0;slot<4;slot++) {
    const player=snapshot.players.find(p=>p.slot===slot);
    const row=doc.createElement('article'); row.dataset.slot=String(slot);
    const title=doc.createElement('h3');
    title.textContent=player ? `${player.name}${player.peer===localPeer?' (You)':''}` : `Player ${slot+1} · Open slot`;
    row.append(title);
    if(player) {
      const bar=doc.createElement('progress'); bar.max=1; bar.value=player.progress;
      bar.setAttribute('aria-label',`${player.name} asset download`);
      const detail=doc.createElement('p');
      detail.textContent=`${Math.floor(player.progress*100)}% · ${(player.bytes/1024**2).toFixed(1)} / ${(snapshot.total/1024**2).toFixed(1)} MiB · ${player.phase}`;
      row.append(bar,detail);
    }
    fragment.append(row);
  }
  container.replaceChildren(fragment);
}
