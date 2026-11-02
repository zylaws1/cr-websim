// Collect self-play experience for PPO. Each worker plays matches with the learner policy on one or both sides.
// usage: node tools/ai/rollout.mjs <weightsBase> <outDir> <matches> <opponent: rule|self|weightsBase[,weightsBase…]> [ruleShare=0.5] [greedy=0]
import { Worker, isMainThread, parentPort, workerData } from 'node:worker_threads';
import os from 'node:os';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const DECIDE = 10;                                    // decide every 10 ticks = 0.5 s
if (!isMainThread) {
  const { makeEnv } = await import('../sim/env.mjs');
  const { loadWeights, makePolicy, sample } = await import('./policy.mjs');
  const env = await makeEnv(workerData.seed), S = env.sim.ctx;
  const D = env.metaDecks().map(d => d[1]);
  const OBS = S.NN_OBS, T = S.NN_TILES, K = S.NN_KINDS, CF = S.NN_CARD, HAND = OBS - 5 * CF;
  const learner = makePolicy(loadWeights(workerData.weights));
  const opps = (workerData.opponents || []).map(b => makePolicy(loadWeights(b)));
  let rs = workerData.seed * 9973 + 1; const rand = () => { rs = (rs * 1103515245 + 12345) & 0x7fffffff; return rs / 0x7fffffff; };
  const kindMask = new Uint8Array(K), tileMask = new Uint8Array(4 * T);
  // reward: tower HP swing (a whole tower = 1/3) + trade bonus: troop damage dealt minus taken (TRADE per 1000 HP)
  // v3: trade bonus counts elixir value destroyed (a 3-elixir unit fully killed = 3), not raw HP
  const TRADE = +(process.env.CR_TRADE ?? 0.03);
  const stepReward = (p, c) => (p.theirs - c.theirs) - (p.mine - c.mine) + TRADE * ((c.exTheirs - p.exTheirs) - (c.exMine - p.exMine));
  // returns an agent function; if `rec` is given, records transitions
  const agentFor = (pol, rec, greedy) => {
    let tick = 0, prev = null;
    const fn = side => {
      if (tick++ % DECIDE) return;
      const obs = S.nnObs(side); S.nnMasks(side, kindMask, tileMask);
      const sc = S.nnScore(side);
      if (rec && prev) rec.rew.push(stepReward(prev, sc));
      prev = sc;
      const tr = pol.trunk(obs);
      const [k, lk] = sample(tr.kind, kindMask, 0, K, rand, greedy ? 0 : 1);
      let tile = 0, lt = 0;
      if (k >= 1 && k <= 4) [tile, lt] = sample(pol.tile(obs.subarray(HAND + (k - 1) * CF, HAND + k * CF)), tileMask, (k - 1) * T, T, rand, greedy ? 0 : 1);
      if (rec) {
        rec.obs.push(Float32Array.from(obs)); rec.kmask.push(Uint8Array.from(kindMask));
        rec.tmask.push(k >= 1 && k <= 4 ? tileMask.slice((k - 1) * T, k * T) : new Uint8Array(T));
        rec.kind.push(k); rec.tile.push(tile); rec.logp.push(lk + lt); rec.val.push(tr.value);
      }
      S.nnApply(side, k, tile);
    };
    fn.finish = (side, result) => {                     // terminal reward
      if (!rec || !prev) return;
      const sc = S.nnScore(side), won = result === (side === 'player' ? 'A' : 'B'), lost = result === (side === 'player' ? 'B' : 'A');
      rec.rew.push(stepReward(prev, sc) + (won ? 1 : lost ? -1 : 0));
    };
    return fn;
  };
  parentPort.on('message', job => {
    rs = (job.seed * 7919 + 13) & 0x7fffffff;              // policy sampling depends only on the match, not the worker
    const recs = [], A = job.sideA === 'learner', B = job.sideB === 'learner';
    const mk = (which, side) => {
      if (which === 'learner') { const rec = { obs: [], kmask: [], tmask: [], kind: [], tile: [], logp: [], val: [], rew: [], side }; recs.push(rec); return agentFor(learner, rec, job.greedy); }
      if (which === 'rule') return 'rule';
      return agentFor(opps[+which.slice(4)], null, false);          // 'opp:N'
    };
    const deckA = D[job.da % D.length], deckB = D[job.db % D.length];
    const fa = mk(job.sideA, 'player'), fb = mk(job.sideB, 'enemy');
    env.reseed(job.seed);
    const res = env.match({ deckA, deckB, agentA: fa, agentB: fb });
    if (typeof fa === 'function') fa.finish('player', res.winner);
    if (typeof fb === 'function') fb.finish('enemy', res.winner);
    parentPort.postMessage({ res, recs: recs.map(r => ({ ...r, n: r.kind.length })), job });
  });
  parentPort.postMessage({ ready: true });
} else {
  const [weights, outDir, nMatches, oppSpec = 'rule', ruleShare = '0.5', greedy = '0', seedBase = '1'] = process.argv.slice(2);
  const opponents = oppSpec === 'rule' || oppSpec === 'self' ? [] : oppSpec.split(',');
  if (process.env.CR_FIXED_JOBS) {                         // deterministic job list (benchmark)
    let r = +seedBase; Math.random = () => { r = (r * 1103515245 + 12345) & 0x7fffffff; return r / 0x7fffffff; };
  }
  const jobs = [];
  for (let i = 0; i < +nMatches; i++) {
    const vsRule = oppSpec === 'rule' || Math.random() < +ruleShare;
    const opp = vsRule ? 'rule' : opponents.length ? 'opp:' + Math.floor(Math.random() * opponents.length) : 'learner';
    const learnerA = Math.random() < 0.5;
    jobs.push({ da: Math.floor(Math.random() * 30), db: Math.floor(Math.random() * 30), sideA: learnerA ? 'learner' : opp, sideB: learnerA ? opp : 'learner',
      seed: +seedBase * 100000 + i, greedy: +greedy, vsRule });
  }
  const file = fileURLToPath(import.meta.url), out = [];
  let next = 0;
  await Promise.all(Array.from({ length: Math.min((+process.env.CR_THREADS || os.cpus().length), jobs.length) }, (_, w) => new Promise((res, rej) => {
    const wk = new Worker(file, { workerData: { seed: +seedBase * 100 + w + 1, weights, opponents } });
    const feed = () => { if (next >= jobs.length) { wk.terminate(); res(); return; } wk.postMessage(jobs[next++]); };
    wk.on('message', m => { if (m.ready) return feed(); out.push(m); feed(); });
    wk.on('error', rej);
  })));
  // pack trajectories into flat binary arrays for the trainer
  const recs = out.flatMap(m => m.recs), N = recs.reduce((a, r) => a + r.n, 0);
  const OBS = recs[0] ? recs[0].obs[0].length : 0, T = recs[0] ? recs[0].tmask[0].length : 0;
  const obs = new Float32Array(N * OBS), km = new Uint8Array(N * 6), tm = new Uint8Array(N * T), kind = new Int32Array(N), tile = new Int32Array(N),
    logp = new Float32Array(N), val = new Float32Array(N), rew = new Float32Array(N), done = new Uint8Array(N);
  let o = 0;
  for (const r of recs) for (let i = 0; i < r.n; i++, o++) {
    obs.set(r.obs[i], o * OBS); km.set(r.kmask[i], o * 6); tm.set(r.tmask[i], o * T);
    kind[o] = r.kind[i]; tile[o] = r.tile[i]; logp[o] = r.logp[i]; val[o] = r.val[i]; rew[o] = r.rew[i] ?? 0; done[o] = i === r.n - 1 ? 1 : 0;
  }
  fs.mkdirSync(outDir, { recursive: true });
  for (const [k, a] of Object.entries({ obs, km, tm, kind, tile, logp, val, rew, done })) fs.writeFileSync(path.join(outDir, k + '.bin'), Buffer.from(a.buffer));
  // stats: learner win rate vs rule and vs others
  const st = { n: N, obs: OBS, tiles: T, matches: out.length, vsRule: [0, 0], vsSelf: [0, 0], crowns: 0, cards: 0 };
  for (const m of out) {
    const learnerSide = m.job.sideA === 'learner' ? 'A' : m.job.sideB === 'learner' ? 'B' : null;
    if (!learnerSide) continue;
    const w = m.res.winner === learnerSide ? 1 : m.res.winner === 'draw' ? 0.5 : 0, key = m.job.vsRule ? 'vsRule' : 'vsSelf';
    st[key][0] += w; st[key][1]++; st.crowns += learnerSide === 'A' ? m.res.crownsA : m.res.crownsB;
  }
  st.cards = kind.reduce((a, k) => a + (k >= 1 && k <= 4 ? 1 : 0), 0);
  fs.writeFileSync(path.join(outDir, 'meta.json'), JSON.stringify(st));
  console.log(JSON.stringify(st));
}
