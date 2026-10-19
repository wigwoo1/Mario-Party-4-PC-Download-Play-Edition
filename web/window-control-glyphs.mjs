// Verified against WIN_ICON_ANM bank/frame/layer metadata and bitmap preview.
// These names denote emulated game actions, never physical button positions.
const actions=new Map([[2,'PAD_BUTTON_A'],[3,'PAD_BUTTON_B'],
  [4,'PAD_BUTTON_X'],[5,'PAD_BUTTON_Y'],[6,'PAD_TRIGGER_R'],
  [7,'PAD_TRIGGER_R'],[8,'PAD_TRIGGER_L'],[9,'PAD_TRIGGER_L'],[11,'PAD_TRIGGER_Z'],[12,'PAD_BUTTON_MENU']]);

// Call only for the window icon animation, not an arbitrary sprite bank.
// The renderer retains the original whenever this returns null.
export function windowControlGlyph(bank,frame,resolveAction) {
  if(!Number.isInteger(bank)||!Number.isInteger(frame)||frame<0)return null;
  const action=actions.get(bank);
  if(!action)return null;
  const animated=bank===7||bank===9;
  if(frame>=(animated?2:1))return null; // End sentinel is not a drawable frame.
  const glyph=resolveAction(action);
  if(!glyph)return null;
  return {...glyph,action,pressed:animated&&frame===1};
}
