'use strict';
// ICE gathering completes independently for each peer. Preserve each original
// invitation and select by its assigned identity, never by completion order.
function orderInitialInvitations(invitations,count){
 if(!Number.isInteger(count)||count<1||count>3||!Array.isArray(invitations)||invitations.length!==count)throw Error('Complete bounded initial invitation set required');
 const byPeer=new Map();
 for(const invitation of invitations){
  if(invitation?.type!=='initial-invitation'||!Array.from({length:count},(_,index)=>'guest'+(index+1)).includes(invitation.peer)||byPeer.has(invitation.peer))throw Error('Unexpected or duplicate initial invitation peer');
  byPeer.set(invitation.peer,invitation);
 }
 return Array.from({length:count},(_,index)=>byPeer.get('guest'+(index+1)));
}
module.exports={orderInitialInvitations};
