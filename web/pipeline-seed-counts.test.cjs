'use strict';
const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { DatabaseSync } = require('node:sqlite');
const { countPipelineSeedDefinitions } = require('./pipeline-seed-counts.cjs');

test('counts graphics and UI rows separately from the total seed definitions', () => {
  const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'partyboard-seed-counts-'));
  const databasePath = path.join(directory, 'seed.db');
  try {
    const database = new DatabaseSync(databasePath);
    database.exec('CREATE TABLE pipeline_cache(type INTEGER NOT NULL, hash INTEGER NOT NULL, PRIMARY KEY(type, hash))');
    const insert = database.prepare('INSERT INTO pipeline_cache(type, hash) VALUES (?, ?)');
    for (let hash = 1; hash <= 3; hash++) insert.run(1, hash);
    for (let hash = 1; hash <= 2; hash++) insert.run(2, hash);
    database.close();
    assert.deepEqual(countPipelineSeedDefinitions(databasePath), { graphics: 3, ui: 2, total: 5 });
  } finally {
    fs.rmSync(directory, { recursive: true, force: true });
  }
});

test('rejects unknown definition types rather than hiding them in the graphics count', () => {
  const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'partyboard-seed-counts-'));
  const databasePath = path.join(directory, 'seed.db');
  try {
    const database = new DatabaseSync(databasePath);
    database.exec('CREATE TABLE pipeline_cache(type INTEGER NOT NULL, hash INTEGER NOT NULL, PRIMARY KEY(type, hash))');
    database.prepare('INSERT INTO pipeline_cache(type, hash) VALUES (?, ?)').run(3, 1);
    database.close();
    assert.throws(() => countPipelineSeedDefinitions(databasePath), /Unsupported shader seed definition type 3/);
  } finally {
    fs.rmSync(directory, { recursive: true, force: true });
  }
});
