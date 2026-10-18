// Only accept this message on the already bound host start-control channel.
export function receiveSessionInputLoss(message,{session,plan}){
 if(message?.type!=='session-input-loss'||message.version!==1||message.session!==session||
    typeof message.peer!=='string'||!plan.bindings.some(binding=>binding.peer===message.peer&&binding.peer!==plan.peer))
  throw Error('Invalid session input loss notice.');
 return message.peer;
}
export function sessionInputLossNotice(peer,{session,plan}){
 const notice={type:'session-input-loss',version:1,session,peer};
 receiveSessionInputLoss(notice,{session,plan});return notice;
}
