export function lobbyMusic(audio, button, label, volume, storage) {
  const tracks = [
    ['./audio/wifi-menu.opus', 'WIFI Menu'],
    ['./audio/currently-spectating.opus', 'Currently Spectating!'],
  ];
  let active = false, index = 0, revision = 0, needsPlay = true;
  const key = 'partyboard.lobbyMusic';
  const validVolume = value => typeof value === 'number' && Number.isFinite(value) && value >= 0 && value <= 1;
  let preferences = {volume: validVolume(Number(volume.value)) ? Number(volume.value) : 0.3, muted: false};
  try {
    const saved = JSON.parse(storage?.getItem(key) ?? 'null');
    if (saved && validVolume(saved.volume) && typeof saved.muted === 'boolean')
      preferences = {volume: saved.volume, muted: saved.muted};
  } catch {}
  audio.preload = 'none';
  volume.value = String(preferences.volume);
  audio.volume = preferences.volume;
  audio.muted = preferences.muted;
  function refresh() {
    button.textContent = audio.muted ? 'Unmute music' : 'Mute music';
    button.setAttribute?.('aria-pressed', String(audio.muted));
  }
  function save() { try { storage?.setItem(key, JSON.stringify(preferences)); } catch {} }
  refresh();
  async function play() {
    const current = ++revision;
    try {
      await audio.play();
      if (current === revision && active) { needsPlay = false; label.textContent = tracks[index][1]; refresh(); }
    } catch {
      if (current === revision && active) { needsPlay = true; label.textContent = 'Select Play music to start audio.'; button.textContent = 'Play music'; }
    }
  }
  audio.addEventListener('ended', () => {
    if (!active) return;
    index = (index + 1) % tracks.length; audio.src = tracks[index][0]; void play();
  });
  audio.addEventListener('error', () => {
    if (active) { revision++; needsPlay = true; label.textContent = 'Music unavailable. Download can continue.'; button.textContent = 'Play music'; }
  });
  button.onclick = () => {
    if (!active) return;
    if (audio.paused || needsPlay) void play();
    else { preferences.muted = audio.muted = !audio.muted; save(); refresh(); }
  };
  volume.oninput = () => {
    const value = Number(volume.value);
    if (!validVolume(value)) { volume.value = String(preferences.volume); return; }
    preferences.volume = audio.volume = value; save();
  };
  return {
    start() {
      active = true; needsPlay = true; index = 0; button.disabled = false;
      audio.src = tracks[index][0]; void play();
    },
    stop() {
      active = false; revision++; audio.pause(); audio.removeAttribute('src'); audio.load();
      button.disabled = true; label.textContent = 'Music starts when you join.';
    },
  };
}
