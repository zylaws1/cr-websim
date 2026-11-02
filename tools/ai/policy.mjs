// Policy network forward pass in plain JS (same layout as tools/ai/model.py). Weights: <name>.json (shapes) + <name>.bin (float32).
//   trunk: obs → 256 → 256 (ReLU) = h;  kind head: h → 6;  value: h → 1;  tile head: [h, card features] → 256 (ReLU) → 540
import fs from 'node:fs';
export const LAYERS = ['t1', 't2', 'kind', 'val', 'p1', 'p2'];
export function loadWeights(base) {
  const meta = JSON.parse(fs.readFileSync(base + '.json', 'utf8')), buf = fs.readFileSync(base + '.bin');
  const f = new Float32Array(buf.buffer, buf.byteOffset, buf.byteLength / 4);
  const W = { meta }; let o = 0;
  for (const L of LAYERS) {
    const [out, inp] = meta.shapes[L];
    W[L] = { w: f.subarray(o, o + out * inp), b: f.subarray(o + out * inp, o + out * inp + out), out, inp }; o += out * inp + out;
  }
  return W;
}
function linear(L, x, y, relu) {
  const { w, b, out, inp } = L;
  for (let i = 0; i < out; i++) {
    let s = b[i]; const r = i * inp;
    for (let j = 0; j < inp; j++) s += w[r + j] * x[j];
    y[i] = relu && s < 0 ? 0 : s;
  }
  return y;
}
export function makePolicy(W) {
  const H = W.t1.out, h1 = new Float32Array(H), h = new Float32Array(H), kind = new Float32Array(W.kind.out), val = new Float32Array(1);
  const pin = new Float32Array(W.p1.inp), p1 = new Float32Array(W.p1.out), tile = new Float32Array(W.p2.out);
  return {
    trunk(obs) { linear(W.t1, obs, h1, true); linear(W.t2, h1, h, true); linear(W.kind, h, kind, false); linear(W.val, h, val, false); return { h, kind, value: val[0] }; },
    tile(cardFeat) { pin.set(h, 0); pin.set(cardFeat, H); linear(W.p1, pin, p1, true); return linear(W.p2, p1, tile, false); },
  };
}
// masked softmax sample (greedy if temp === 0); returns [index, logp]
export function sample(logits, mask, maskOff, n, rand, temp = 1) {
  let mx = -Infinity;
  for (let i = 0; i < n; i++) if (mask[maskOff + i] && logits[i] > mx) mx = logits[i];
  let z = 0; for (let i = 0; i < n; i++) if (mask[maskOff + i]) z += Math.exp(logits[i] - mx);
  let pick = -1;
  if (temp === 0) { let best = -Infinity; for (let i = 0; i < n; i++) if (mask[maskOff + i] && logits[i] > best) { best = logits[i]; pick = i; } }
  else { let u = rand() * z; for (let i = 0; i < n; i++) if (mask[maskOff + i]) { u -= Math.exp(logits[i] - mx); pick = i; if (u <= 0) break; } }
  return [pick, logits[pick] - mx - Math.log(z)];
}
