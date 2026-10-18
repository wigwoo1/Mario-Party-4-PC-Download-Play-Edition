const ICE_URL = /^(?:stun|stuns|turn|turns):[^\s]{1,250}$/i;

/** Build the ICE configuration from local settings; these credentials are never put in invitation codes. */
export function iceServerConfiguration({ stun = '', turn = '', username = '', credential = '' } = {}) {
  const server = (value, protocol, kind) => {
    if (typeof value !== 'string') throw new TypeError('ICE server URLs must be text.');
    const url = value.trim();
    if (url && !ICE_URL.test(url)) throw new Error('Enter a valid STUN or TURN URL.');
    if (url && !protocol.test(url)) throw new Error(`Use a ${kind} URL for this setting.`);
    return url;
  };
  stun = server(stun, /^stuns?:/i, 'STUN');
  turn = server(turn, /^turns?:/i, 'TURN');
  if (typeof username !== 'string' || typeof credential !== 'string' ||
      username.length > 256 || credential.length > 256 || /[\r\n]/.test(username + credential)) {
    throw new Error('TURN credentials are invalid.');
  }
  username = username.trim();
  credential = credential.trim();
  if ((username || credential) && !turn) throw new Error('Add a TURN relay URL before entering credentials.');
  if (turn && (!username || !credential)) throw new Error('Enter both TURN username and credential.');

  const iceServers = [];
  if (stun) iceServers.push({ urls: stun });
  if (turn) iceServers.push({ urls: turn, username, credential });
  return iceServers;
}
