// Behaviour-cloning data: record the rule AI's decisions in the neural agent's action space.
// usage: node tools/ai/bc_collect.mjs <outDir> <matches>
import { Worker, isMainThread, parentPort, workerData } from 'node:worker_threads';
import os from 'node:os';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
const DECIDE = 10;
if (!isMainThread) {
  const { makeEnv } = await import('../sim/env.mjs');
  const env = await makeEnv(workerData.seed), S = env.sim.ctx;
  const D = env.metaDecks().map(d => d[1]);
  const T = S.NN_TILES, K = S.NN_KINDS, handOf = env.sim.run('handOf'), ROWS = env.sim.run('ROWS'), COLS = env.sim.run('COLS');
  const origPlay = S.playCard, origAbility = S.useAbility;
  let capture = null;
  S.playCard = (side, card, x, y) => {
    const eff = S.effectiveCard(card, side), p = eff && S.resolvePlacement(side, eff, x, y);
    const ok = origPlay(side, card, x, y);
    if (ok && capture && capture.side === side && p) {
      const r = side === 'player' ? p.row : ROWS - 1 - p.row;
      capture.kind = 1 + capture.hand.indexOf(card); capture.tile = p.col + COLS * r;
    }
    return ok;
  };
  S.useAbility = side => { const ok = origAbility(side); if (ok && capture && capture.side === side) capture.kind = 5; return ok; };
  parentPort.on('message', job => {
    const rec = { obs: [], km: [], tm: [], kind: [], tile: [] };
    const agent = () => { let t = 0; return side => {
      if (t++ % DECIDE) return;
      const obs = S.nnObs(side), { kindMask, tileMask } = S.nnMasks(side);
      capture = { side, hand: [...handOf(side)], kind: 0, tile: 0 };
      S.ruleAgent(side, DECIDE * 0.05);
      const c = capture; capture = null;
      if (c.kind >= 1 && c.kind <= 4 && !tileMask[(c.kind - 1) * T + c.tile]) return;   // resolved off the legal grid (rare): skip
      if (c.kind === 0 && Math.random() > 0.25) return;                                   // keep a quarter of the waits
      rec.obs.push(Float32Array.from(obs)); rec.km.push(Uint8Array.from(kindMask));
      rec.tm.push(c.kind >= 1 && c.kind <= 4 ? tileMask.slice((c.kind - 1) * T, c.kind * T) : new Uint8Array(T));
      rec.kind.push(c.kind); rec.tile.push(c.tile);
    }; };
    env.reseed(job.seed);
    env.match({ deckA: D[job.da], deckB: D[job.db], agentA: agent(), agentB: agent() });
    parentPort.postMessage(rec);
  });
  parentPort.postMessage({ ready: true });
} else {
  const [outDir, n] = process.argv.slice(2);
  const jobs = Array.from({ length: +n }, (_, i) => ({ da: i % 7, db: Math.floor(i / 7) % 7, seed: 777000 + i }));
  const file = fileURLToPath(import.meta.url), out = []; let next = 0;
  await Promise.all(Array.from({ length: (+process.env.CR_THREADS || os.cpus().length) }, (_, w) => new Promise((res, rej) => {
    const wk = new Worker(file, { workerData: { seed: 500 + w } });
    const feed = () => { if (next >= jobs.length) { wk.terminate(); res(); return; } wk.postMessage(jobs[next++]); };
    wk.on('message', m => { if (m.ready) return feed(); out.push(m); feed(); }); wk.on('error', rej);
  })));
  const N = out.reduce((a, r) => a + r.kind.length, 0), OBS = out[0].obs[0].length, T = out[0].tm[0].length;
  const obs = new Float32Array(N * OBS), km = new Uint8Array(N * 6), tm = new Uint8Array(N * T), kind = new Int32Array(N), tile = new Int32Array(N);
  let o = 0;
  for (const r of out) for (let i = 0; i < r.kind.length; i++, o++) { obs.set(r.obs[i], o * OBS); km.set(r.km[i], o * 6); tm.set(r.tm[i], o * T); kind[o] = r.kind[i]; tile[o] = r.tile[i]; }
  fs.mkdirSync(outDir, { recursive: true });
  for (const [k, a] of Object.entries({ obs, km, tm, kind, tile })) fs.writeFileSync(path.join(outDir, k + '.bin'), Buffer.from(a.buffer));
  const counts = [0, 0, 0, 0, 0, 0]; for (const k of kind) counts[k]++;
  fs.writeFileSync(path.join(outDir, 'meta.json'), JSON.stringify({ n: N, counts }));
  console.log(JSON.stringify({ n: N, counts }));
}
