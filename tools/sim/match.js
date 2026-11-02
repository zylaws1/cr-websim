// In-game helpers for headless matches (evaluated inside the game's global scope by tools/sim/env.mjs).
// agents: 'rule' | 'idle' | function(side, dt) called every tick.
var __agents = { player: null, enemy: null };
function simMatch(o) {
  const card = id => { const c = ALL_CARDS.find(c => c.id === id); if (!c) throw new Error('unknown card ' + id); return c; };
  towerTroop.player = o.towerA || 'tower-princess';
  pickAiTowerTroop = () => o.towerB || 'tower-princess';
  setLevel('player', o.levelA || 11); setLevel('enemy', o.levelB || 11);
  chosenDeck = o.deckA.map(card);
  startBattle(); loopToken++;
  setSideDeck('enemy', o.deckB.map(card));
  const wrap = f => (side, dt) => f(dt);
  const pick = (a, side) => a === 'rule' ? (s, dt) => ruleAgent(s, dt) : a === 'nn' ? wrap(nnAgent(side)) : a === 'search' ? wrap(searchAgent(side, (side === 'player' ? o.searchOptsA : o.searchOptsB) || o.searchOpts || {}))
    : a === 'idle' || !a ? () => {} : a;
  const A = pick(o.agentA, 'player'), B = pick(o.agentB, 'enemy');
  aiTick = () => {};
  const dt = 0.05, maxT = (180 + 120) / dt + 10;
  let n = 0;
  while (gameRunning && n < maxT) { A('player', dt); B('enemy', dt); tick(dt); n++; }
  const hp = s => Object.values(towers).filter(t => t.side === s).reduce((a, t) => a + Math.max(0, t.hp), 0);
  const winner = pCrowns > eCrowns ? 'A' : eCrowns > pCrowns ? 'B' : lastWinner === 'player' ? 'A' : lastWinner === 'enemy' ? 'B' : 'draw';
  return { winner, crownsA: pCrowns, crownsB: eCrowns, time: +(n * dt).toFixed(1), towerHpA: hp('player'), towerHpB: hp('enemy') };
}
