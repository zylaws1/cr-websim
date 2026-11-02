// Meta deck leaderboard: every meta deck plays every other (both sides) with the given agents.
// usage: node tools/sim/leaderboard.mjs [gamesPerPairing=40] [agent=rule]
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { makeEnv } from './env.mjs';
import { runJobs } from './pool.mjs';
const here = path.dirname(fileURLToPath(import.meta.url));
const N = +(process.argv[2] || 40), agent = process.argv[3] || 'rule';
const env = await makeEnv(1);
const decks = env.metaDecks();
const jobs = [];
for (let a = 0; a < decks.length; a++) for (let b = 0; b < decks.length; b++) if (a !== b)
  for (let g = 0; g < N / 2; g++) jobs.push({ deckA: decks[a][1], deckB: decks[b][1], agentA: agent, agentB: agent, a, b, seed: jobs.length + 1 });
const t0 = Date.now();
const res = await runJobs(jobs, { onProgress: (d, n) => { if (d % 200 === 0 || d === n) process.stderr.write(`\r${d}/${n} matches`); } });
process.stderr.write(`  (${((Date.now() - t0) / 1000).toFixed(1)} s)\n`);
const W = decks.map(() => decks.map(() => [0, 0]));            // [wins, games] for row deck vs column deck
res.forEach((r, i) => {
  const { a, b } = jobs[i], sa = r.winner === 'A' ? 1 : r.winner === 'draw' ? 0.5 : 0;
  W[a][b][0] += sa; W[a][b][1]++; W[b][a][0] += 1 - sa; W[b][a][1]++;
});
const rows = decks.map((d, i) => {
  const tot = W[i].reduce((s, [w, g]) => [s[0] + w, s[1] + g], [0, 0]);
  return { deck: d[0], sim: +(100 * tot[0] / tot[1]).toFixed(1), royaleapi: d[3], games: tot[1], vs: W[i].map(([w, g]) => (g ? Math.round(100 * w / g) : null)) };
}).sort((x, y) => y.sim - x.sim);
const short = s => s.slice(0, 12);
let out = `# Meta deck leaderboard (${agent} AI, ${N} games per pairing, ${res.length} matches)\n\n| # | Deck | Sim win % | RoyaleAPI win % |\n|---|---|---|---|\n`;
rows.forEach((r, i) => { out += `| ${i + 1} | ${r.deck} | ${r.sim} | ${r.royaleapi} |\n`; });
out += `\nRow deck's win % against each column deck:\n\n| | ${decks.map(d => short(d[0])).join(' | ')} |\n|---|${decks.map(() => '---').join('|')}|\n`;
decks.forEach((d, i) => { out += `| ${short(d[0])} | ${W[i].map(([w, g]) => (g ? Math.round(100 * w / g) : '—')).join(' | ')} |\n`; });
const xs = rows.map(r => r.sim), ys = rows.map(r => r.royaleapi), m = v => v.reduce((a, b) => a + b, 0) / v.length;
const cov = xs.reduce((s, x, i) => s + (x - m(xs)) * (ys[i] - m(ys)), 0), corr = cov / Math.sqrt(xs.reduce((s, x) => s + (x - m(xs)) ** 2, 0) * ys.reduce((s, y) => s + (y - m(ys)) ** 2, 0));
out += `\nCorrelation with RoyaleAPI win rates: ${corr.toFixed(2)}\n`;
const dir = path.join(here, '..', '..', 'reports'); fs.mkdirSync(dir, { recursive: true });
fs.writeFileSync(path.join(dir, `leaderboard-${agent}.md`), out);
console.log(out);
