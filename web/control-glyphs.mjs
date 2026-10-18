// Input routing supplies the physical binding; GameCube action names must not
// be used as physical labels (Xbox and Switch face layouts differ).
const labels={
  xbox:{south:'A',east:'B',west:'X',north:'Y',l1:'LB',r1:'RB',l2:'LT',r2:'RT',start:'Menu',select:'View'},
  playstation:{south:'Cross',east:'Circle',west:'Square',north:'Triangle',l1:'L1',r1:'R1',l2:'L2',r2:'R2',start:'Options',select:'Share'},
  switch:{south:'B',east:'A',west:'Y',north:'X',l1:'L',r1:'R',l2:'ZL',r2:'ZR',start:'+',select:'−'},
  generic:{south:'1',east:'2',west:'3',north:'4',l1:'L1',r1:'R1',l2:'L2',r2:'R2',start:'Start',select:'Select'}
};
const escape=text=>text.replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&apos;'}[c]));
const shapes={
  Cross:'<path d="M22 22L42 42M42 22L22 42"/>',
  Circle:'<circle cx="32" cy="32" r="14"/>',
  Square:'<rect x="19" y="19" width="26" height="26" rx="1"/>',
  Triangle:'<path d="M32 17L48 44H16Z"/>'
};
export function controlGlyph({family='generic',binding}) {
  if(!binding)throw TypeError('A physical binding is required.');
  const keyboard=binding.kind==='key';
  if(!keyboard && binding.kind!=='button')throw TypeError('Unsupported input binding.');
  if(!keyboard && !Object.hasOwn(labels,family))throw TypeError('Unknown controller family.');
  const label=keyboard?binding.label:labels[family][binding.button];
  if(typeof label!=='string'||!label.trim()||[...label].length>16||/[\u0000-\u001f]/.test(label))throw TypeError('Invalid control label.');
  const outline=keyboard?'<rect x="4" y="8" width="56" height="48" rx="8"/>':'<circle cx="32" cy="32" r="28"/>';
  const symbol=!keyboard&&family==='playstation'?shapes[label]:null;
  const content=symbol?`<g fill="none" stroke="#fff" stroke-width="3" stroke-linejoin="round" stroke-linecap="round">${symbol}</g>`:
    `<text x="32" y="33" text-anchor="middle" dominant-baseline="middle" font-family="Arial,sans-serif" font-weight="bold" font-size="${label.length>5?10:label.length>2?14:25}" fill="#fff">${escape(label)}</text>`;
  return {label,svg:`<svg xmlns="http://www.w3.org/2000/svg" width="64" height="64" viewBox="0 0 64 64" role="img"><title>${escape(label)}</title><g fill="#202936" stroke="#fff" stroke-width="2.5">${outline}</g>${content}</svg>`};
}
export function actionGlyph(action,{family,bindings}) {
  if(!bindings||!Object.hasOwn(bindings,action))return null;
  return controlGlyph({family,binding:bindings[action]});
}
