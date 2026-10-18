// Deterministic test stimulus only; never selected by production input code.
export function diagnosticPad(peer,poll,mode='stimulus',slot=peer===11?0:1) {
  if(![11,22].includes(peer)||!Number.isInteger(poll)||poll<0)throw Error('Diagnostic peer and poll required.');
  if(['startup','party','fourhuman','characters','board','boardturn'].includes(mode)) {
    const pad=new Uint8Array(11),pressed=start=>poll>=start&&poll<start+20;
    if(peer===11&&pressed(1000))pad[1]=16;
    if(mode!=='startup'&&peer===11&&[2070,2250].some(pressed))pad[1]=1;
    if(['fourhuman','characters','board','boardturn'].includes(mode)&&peer===11&&[3400,3700,4000].some(pressed))pad[1]=1;
    if(['characters','board','boardturn'].includes(mode)&&([4600,4900].some(pressed)||pressed([5100,5200,5300,5400][slot])||(slot===0&&pressed(5600))))pad[1]=1;
    if(['board','boardturn'].includes(mode)&&slot===0&&[6600,6900,7200,7500,7800,8100,8400,8700,9000,9300].some(pressed))pad[1]=1;
    if(mode==='boardturn'&&poll>=10200&&poll<11800&&poll%200<20)pad[1]=1;
    if(mode==='fourstimulus'){pad[2]=(pad[2]+slot)&255;pad[4]=(pad[4]+slot*3)&255;}
  return pad;
  }
  const pad=new Uint8Array(11),button=poll%16<4?(peer===11?0x100:0x200):0;
  pad[0]=button&255;pad[1]=button>>>8;
  pad[2]=peer===11?196:60;pad[3]=poll%2?40:216;
  pad[4]=peer===11?30:226;pad[5]=poll%2?20:236;
  pad[6]=poll%2?64:192;pad[7]=peer===11?32:96;
  if(mode==='fourstimulus'){pad[2]=(pad[2]+slot)&255;pad[4]=(pad[4]+slot*3)&255;}
  return pad;
}
