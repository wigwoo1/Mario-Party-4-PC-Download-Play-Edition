const assert = require('node:assert/strict');

function validateBoardMovement(snapshots) {
  const first = snapshots.findIndex(state => state.player >= 0 && state.player < 4);
  assert.ok(first >= 0, 'A player turn must begin');
  const states = snapshots.slice(first);
  const baseline = states[0];
  assert.equal(new Set(baseline.characters).size, 4, 'Four distinct characters must be initialized');
  assert.ok(baseline.spaces.every(space => space > 0), 'Starting spaces must be initialized');
  const moved = new Set();
  const active = new Set();
  let previous = baseline;
  for (const state of states) {
    assert.ok(state.frame >= previous.frame, 'Snapshots must remain ordered');
    assert.equal(state.turn, baseline.turn, 'Probe must remain in the first board phase');
    assert.deepEqual(state.characters, baseline.characters, 'Player order must remain stable');
    if (state.player >= 0 && state.player < 4) {
      active.add(state.player);
      if (state.spaces[state.player] !== previous.spaces[state.player]) moved.add(state.player);
    }
    previous = state;
    // Validate the first completed board phase, even when later turns follow.
    if (state.player === -1 && active.size === 4 && moved.size === 4) break;
  }
  assert.equal(active.size, 4, 'All four players must take a turn');
  assert.equal(moved.size, 4, 'Each player must move during their active turn');
  assert.equal(previous.player, -1, 'Final snapshot must leave the individual player turn phase');
  assert.ok(previous.spaces.every((space, index) => space !== baseline.spaces[index]), 'All players must leave their starting spaces');
  return {turn: previous.turn, characters: previous.characters, spaces: previous.spaces};
}

module.exports = {validateBoardMovement};
if (require.main === module) {
  const snapshots = JSON.parse(require('node:fs').readFileSync(process.argv[2], 'utf8'));
  console.log('PASS: Four-player first board phase movement: ' + JSON.stringify(validateBoardMovement(snapshots)));
}
