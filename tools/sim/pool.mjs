// Worker pool: each worker owns a headless game and runs match jobs ({deckA, deckB, agentA, agentB, seed…}).
import { Worker, isMainThread, parentPort, workerData } from 'node:worker_threads';
import os from 'node:os';
import { fileURLToPath } from 'node:url';

if (!isMainThread) {
  const { makeEnv } = await import('./env.mjs');
  const env = await makeEnv(workerData.seed);
  parentPort.on('message', job => {
    try { env.reseed(job.seed); parentPort.postMessage({ id: job.id, res: env.match(job) }); }
    catch (e) { parentPort.postMessage({ id: job.id, err: String(e && e.stack || e) }); }
  });
  parentPort.postMessage({ ready: true });
}

export async function runJobs(jobs, { threads = (+process.env.CR_THREADS || os.cpus().length), onProgress } = {}) {
  const file = fileURLToPath(import.meta.url), results = new Array(jobs.length);
  let next = 0, done = 0;
  await Promise.all(Array.from({ length: Math.min(threads, jobs.length) }, (_, w) => new Promise((resolve, reject) => {
    const wk = new Worker(file, { workerData: { seed: 1000 + w } });
    const feed = () => { if (next >= jobs.length) { wk.terminate(); resolve(); return; } const i = next++; wk.postMessage({ ...jobs[i], id: i, seed: jobs[i].seed ?? i + 1 }); };
    wk.on('message', m => {
      if (m.ready) return feed();
      if (m.err) { reject(new Error(m.err)); return; }
      results[m.id] = m.res; done++; if (onProgress) onProgress(done, jobs.length); feed();
    });
    wk.on('error', reject);
  })));
  return results;
}
