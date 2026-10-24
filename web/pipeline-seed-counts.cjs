'use strict';
const { DatabaseSync } = require('node:sqlite');

function countPipelineSeedDefinitions(databasePath) {
  const database = new DatabaseSync(databasePath, { readOnly: true });
  try {
    const rows = database.prepare(
      'SELECT type, count(*) AS count FROM pipeline_cache GROUP BY type ORDER BY type',
    ).all();
    const counts = { graphics: 0, ui: 0, total: 0 };
    for (const row of rows) {
      if (row.type === 1) counts.graphics = row.count;
      else if (row.type === 2) counts.ui = row.count;
      else throw new Error(`Unsupported shader seed definition type ${row.type}`);
      counts.total += row.count;
    }
    return counts;
  } finally {
    database.close();
  }
}

module.exports = { countPipelineSeedDefinitions };
