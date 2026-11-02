// Fixed benchmark: the same 240 matches every time (fixed seeds and deck pairs over all 30 meta decks, learner on both
// seats) against the rule AI. usage: node tools/ai/bench.mjs <weightsBase> [matches=240]  → prints {"winRate":…}
import { execFileSync } from 'node:child_process';
import path from 'node:path';
import os from 'node:os';
import { fileURLToPath } from 'node:url';
const here = path.dirname(fileURLToPath(import.meta.url));
const [w, n = '240'] = process.argv.slice(2);
// rollout.mjs with a fixed seedBase draws the same jobs (it seeds Math.random below), so results are comparable
const out = execFileSync('node', [path.join(here, 'rollout.mjs'), w, path.join(process.env.CR_RUNS || path.join(os.homedir(), 'cr-ai-runs'), 'benchbatch'), n, 'rule', '1', '0', '424242'],
  { env: { ...process.env, CR_FIXED_JOBS: '1' }, maxBuffer: 1 << 26 }).toString().trim().split('\n').pop();
const st = JSON.parse(out);
console.log(JSON.stringify({ winRate: +(st.vsRule[0] / st.vsRule[1]).toFixed(4), n: st.vsRule[1], crowns: +(st.crowns / st.vsRule[1]).toFixed(2) }));
