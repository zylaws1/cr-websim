// Benchmark any in-game agent ('nn', 'search', 'rule') against another on fixed matches over all 30 meta decks.
// usage: node tools/ai/bench_agents.mjs <agent> [opponent=rule] [matches=240] [agent searchOpts JSON] [opponent searchOpts JSON]
import { makeEnv } from '../sim/env.mjs';
import { runJobs } from '../sim/pool.mjs';
const [agent, opp = 'rule', n = '240', so = '{}', soOpp = '{}'] = process.argv.slice(2);
const env = await makeEnv(1), D = env.metaDecks(), jobs = [];
// paired design: every deck pairing and seed is played twice with the two agents swapped, so deck and seat luck cancel out
for (let i = 0; i < +n / 2; i++) {
  const a = (i * 7) % 30, b0 = (i * 11 + 3 + Math.floor(i / 30)) % 30, b = b0 === a ? (b0 + 1) % 30 : b0;
  for (const learnerA of [true, false])
    jobs.push({ deckA: D[a][1], deckB: D[b][1], agentA: learnerA ? agent : opp, agentB: learnerA ? opp : agent, learnerA, seed: 5000 + i,
      searchOptsA: JSON.parse(learnerA ? so : soOpp), searchOptsB: JSON.parse(learnerA ? soOpp : so) });
}
const t0 = Date.now();
const res = await runJobs(jobs, { onProgress: (d, t) => { if (d % 24 === 0 || d === t) process.stderr.write(`\r${agent} vs ${opp}: ${d}/${t} matches`); } });
let w = 0, cr = 0;
res.forEach((r, i) => { const L = jobs[i].learnerA ? 'A' : 'B'; w += r.winner === L ? 1 : r.winner === 'draw' ? 0.5 : 0; cr += L === 'A' ? r.crownsA : r.crownsB; });
const p = w / res.length;
process.stderr.write('\n');
console.log(JSON.stringify({ agent, opp, winRate: +p.toFixed(4), ci95: +(1.96 * Math.sqrt(p * (1 - p) / res.length)).toFixed(3), crowns: +(cr / res.length).toFixed(2), matches: res.length, secs: (Date.now() - t0) / 1000 }));
