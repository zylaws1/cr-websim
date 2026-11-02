// Runs the game script from index.html in Node with a minimal fake DOM, so matches can be simulated without a browser.
// usage: const sim = await loadGame(); sim.run('code') evaluates inside the game's global scope.
import fs from 'node:fs';
import vm from 'node:vm';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const here = path.dirname(fileURLToPath(import.meta.url));
export const INDEX = path.join(here, '..', '..', 'index.html');

class ClassList {
  constructor() { this.s = new Set(); }
  add(...c) { for (const x of c) this.s.add(x); }
  remove(...c) { for (const x of c) this.s.delete(x); }
  toggle(c, on) { const v = on === undefined ? !this.s.has(c) : !!on; v ? this.s.add(c) : this.s.delete(c); return v; }
  contains(c) { return this.s.has(c); }
}
class El {
  constructor(tag = 'div', id = '') {
    this.tagName = tag.toUpperCase(); this.id = id; this.style = {}; this.dataset = {}; this.classList = new ClassList();
    this.children = []; this._parent = null; this._html = ''; this.textContent = ''; this.title = ''; this.value = '';
    this.width = 400; this.height = 667; this.offsetWidth = 400; this.offsetHeight = 900;
  }
  set className(v) { this.classList = new ClassList(); String(v).split(/\s+/).filter(Boolean).forEach(c => this.classList.add(c)); }
  get className() { return [...this.classList.s].join(' '); }
  set innerHTML(v) { this._html = v; this.children = []; }
  get innerHTML() { return this._html; }
  get parentElement() { return this._parent || (this._parent = new El('div')); }
  set parentElement(p) { this._parent = p; }
  get firstChild() { return this.children[0] || null; }
  get firstElementChild() { return this.children[0] || null; }
  appendChild(c) { c.parentElement = this; this.children.push(c); return c; }
  insertAdjacentHTML() {}
  insertAdjacentText() {}
  remove() { if (this._parent) { const a = this._parent.children, i = a.indexOf(this); if (i >= 0) a.splice(i, 1); this._parent = null; } }
  replaceWith() {}
  querySelector(sel) { this._q = this._q || {}; return this._q[sel] || (this._q[sel] = new El('div')); }
  querySelectorAll() { return []; }
  addEventListener() {}
  removeEventListener() {}
  dispatchEvent() { return true; }
  getBoundingClientRect() { return { left: 0, top: 0, right: 400, bottom: 667, width: 400, height: 667 }; }
  getContext() { return new Proxy({}, { get: (t, k) => (k in t ? t[k] : () => {}), set: (t, k, v) => { t[k] = v; return true; } }); }
  click() {}
  focus() {}
}

export async function loadGame({ seed = 1, model = true } = {}) {
  const html = fs.readFileSync(INDEX, 'utf8');
  const code = html.slice(html.indexOf('<script>') + 8, html.lastIndexOf('</script>'));
  const byId = new Map();
  const document = {
    getElementById: id => { if (!byId.has(id)) byId.set(id, new El('div', id)); return byId.get(id); },
    createElement: tag => new El(tag),
    querySelectorAll: () => [],
    querySelector: () => null,
    addEventListener() {},
    body: new El('body'),
  };
  // deterministic Math.random per context (mulberry32)
  let a = seed >>> 0;
  const rand = () => { a |= 0; a = a + 0x6D2B79F5 | 0; let t = Math.imul(a ^ a >>> 15, 1 | a); t = t + Math.imul(t ^ t >>> 7, 61 | t) ^ t; return ((t ^ t >>> 14) >>> 0) / 4294967296; };
  const M = Object.create(Math); M.random = rand;
  const store = {};
  const ctx = {
    document, Math: M, console, JSON, Object, Array, Set, Map, Number, String, Boolean, Infinity, NaN, isFinite, parseFloat, parseInt, Error, Promise, Proxy,
    localStorage: { getItem: k => store[k] ?? null, setItem: (k, v) => { store[k] = String(v); }, removeItem: k => { delete store[k]; } },
    Audio: function () { return { play: () => Promise.resolve(), pause() {}, set volume(v) {}, set loop(v) {}, set currentTime(v) {} }; },
    performance: { now: () => 0 },
    requestAnimationFrame: () => 0, cancelAnimationFrame() {},
    setTimeout: () => 0, clearTimeout() {}, setInterval: () => 0, clearInterval() {},
    getComputedStyle: () => ({ display: 'block' }),
    innerWidth: 900, innerHeight: 1000,
    addEventListener() {}, MouseEvent: function () {}, DataTransfer: function () {}, atob: s => Buffer.from(s, 'base64').toString('binary'),
  };
  ctx.window = ctx; ctx.globalThis = ctx;
  vm.createContext(ctx);
  const modelFile = process.env.CR_MODEL || path.join(path.dirname(INDEX), 'ai', 'model.js');   // CR_MODEL: evaluate another policy                 // trained policy, as in the browser
  if (model && fs.existsSync(modelFile)) vm.runInContext(fs.readFileSync(modelFile, 'utf8'), ctx, { filename: 'model.js' });
  if (model && process.env.CR_MODEL_B)                                          // second network for head-to-head tests
    vm.runInContext(fs.readFileSync(process.env.CR_MODEL_B, 'utf8').replace('window.NN_MODEL =', 'window.NN_MODEL_B ='), ctx, { filename: 'model_b.js' });
  vm.runInContext(code, ctx, { filename: 'index.html' });
  vm.runInContext('SND.muted = true;', ctx);
  return { ctx, run: src => vm.runInContext(src, ctx), reseed: s => { a = s >>> 0; } };
}
