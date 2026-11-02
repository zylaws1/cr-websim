// Headless match environment: loads the game in Node and runs matches between agents.
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { loadGame } from './headless.mjs';
const here = path.dirname(fileURLToPath(import.meta.url));

export async function makeEnv(seed = 1) {
  const sim = await loadGame({ seed });
  sim.run(fs.readFileSync(path.join(here, 'match.js'), 'utf8'));
  return {
    sim,
    metaDecks: () => sim.run('META_DECKS.map(d => [d[0], d[1], d[2], d[3]])'),
    match: opts => { sim.ctx.__opts = opts; return sim.run('simMatch(__opts)'); },
    reseed: s => sim.reseed(s),
  };
}
