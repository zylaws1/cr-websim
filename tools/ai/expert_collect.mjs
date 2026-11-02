// Expert-iteration data: record the look-ahead search's decisions (network obs → searched move) for training the network.
// usage: node tools/ai/expert_collect.mjs <outDir> <matches> [searchVsSearchShare=0.5]
import { Worker, isMainThread, parentPort, workerData } from 'node:worker_threads';
import os from 'node:os';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
if (!isMainThread) {
  const { makeEnv } = await import('../sim/env.mjs');
  const env = await makeEnv(workerData.seed), S = env.sim.ctx, D = env.metaDecks().map(d => d[1]);
  const T = S.NN_TILES;
  parentPort.on('message', job => {
    const rec = { obs: [], km: [], tm: [], kind: [], tile: [] };
    const expert = side => { let acc = 0; return (s, dt) => {
      if ((acc += dt) < 0.5) return; acc = 0;
      const obs = S.nnObs(side), { kindMask, tileMask } = S.nnMasks(side);
      const c = S.searchChoice(side);
      if (c.searched && c.kind >= 0) {
        rec.obs.push(Float32Array.from(obs)); rec.km.push(Uint8Array.from(kindMask));
        rec.tm.push(c.kind >= 1 && c.kind <= 4 ? tileMask.slice((c.kind - 1) * T, c.kind * T) : new Uint8Array(T));
        rec.kind.push(c.kind); rec.tile.push(c.tile);
      }
      S.nnApply(side, c.kind, c.tile);
    }; };
    env.reseed(job.seed);
    const res = env.match({ deckA: D[job.da], deckB: D[job.db], agentA: expert('player'), agentB: job.both ? expert('enemy') : 'rule' });
    parentPort.postMessage({ rec, res, job });
  });
  parentPort.postMessage({ ready: true });
} else {
  const [outDir, n, share = '0.5'] = process.argv.slice(2);
  const jobs = Array.from({ length: +n }, (_, i) => ({ da: Math.floor(Math.random() * 30), db: Math.floor(Math.random() * 30), both: Math.random() < +share, seed: 900000 + i }));
  const file = fileURLToPath(import.meta.url), out = []; let next = 0, wins = 0, vsRule = 0; const t0 = Date.now();
  await Promise.all(Array.from({ length: +process.env.CR_THREADS || os.cpus().length }, (_, w) => new Promise((res, rej) => {
    const wk = new Worker(file, { workerData: { seed: 700 + w } });
    const feed = () => { if (next >= jobs.length) { wk.terminate(); res(); return; } wk.postMessage(jobs[next++]); };
    wk.on('message', m => {
      if (m.ready) return feed();
      out.push(m.rec);
      const job = m.job;
      if (m.res && !job.both) { vsRule++; wins += m.res.winner === 'A' ? 1 : 0; }
      const el = (Date.now() - t0) / 1000, eta = el / out.length * (jobs.length - out.length);
      process.stderr.write(`\r  collecting expert games ${out.length}/${jobs.length} | decisions ${out.reduce((a, r) => a + r.kind.length, 0)} | ${el.toFixed(0)}s elapsed, ETA ${Math.round(eta)}s   `);
      feed();
    });
    wk.on('error', rej);
  })));
  process.stderr.write('\n');
  const N = out.reduce((a, r) => a + r.kind.length, 0), OBS = out.find(r => r.obs.length).obs[0].length, T = out.find(r => r.tm.length).tm[0].length;
  const obs = new Float32Array(N * OBS), km = new Uint8Array(N * 6), tm = new Uint8Array(N * T), kind = new Int32Array(N), tile = new Int32Array(N);
  let o = 0;
  for (const r of out) for (let i = 0; i < r.kind.length; i++, o++) { obs.set(r.obs[i], o * OBS); km.set(r.km[i], o * 6); tm.set(r.tm[i], o * T); kind[o] = r.kind[i]; tile[o] = r.tile[i]; }
  fs.mkdirSync(outDir, { recursive: true });
  for (const [k, a] of Object.entries({ obs, km, tm, kind, tile })) fs.writeFileSync(path.join(outDir, k + '.bin'), Buffer.from(a.buffer));
  const counts = [0, 0, 0, 0, 0, 0]; for (const k of kind) counts[k]++;
  fs.writeFileSync(path.join(outDir, 'meta.json'), JSON.stringify({ n: N, counts }));
  console.log(JSON.stringify({ n: N, counts, searchVsRuleWin: vsRule ? +(wins / vsRule).toFixed(3) : null }));
}
