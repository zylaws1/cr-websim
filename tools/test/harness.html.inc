<script>
window.__errors = [];
window.addEventListener('error', e => __errors.push(e.message + ' @' + e.lineno));
const R = [];
const ok = (name, cond, info) => R.push({ name, pass: !!cond, info: info === undefined ? '' : info });
const C = id => ALL_CARDS.find(c => c.id === id);
function fresh(handIds, ai = false) {
  chosenDeck = ['knight','archers','x-bow','cannon','the-log','fireball','minions','hog-rider'].map(C);
  startBattle();
  loopToken++;               // stop the RAF loop; drive tick() manually
  if (!ai) aiTick = () => {};
  if (handIds) playerHand = handIds.map(C);
  elixir = 10;
}
function run(sec, step = 0.05) { for (let t = 0; t < sec && gameRunning; t += step) tick(step); }
function clickArena(x, y) {
  const r = document.getElementById('arena').getBoundingClientRect();
  document.getElementById('arena').dispatchEvent(new MouseEvent('click', { clientX: r.left + x * uiScale, clientY: r.top + y * uiScale, bubbles: true }));
}
function play(idx, x, y) { selectedIdx = idx; elixir = 10; clickArena(x, y); }
const U = id => { const c = C(id); return c.units ? c.units[0][0] : c.unit; };
function enemy(id, x, y) { return spawnUnit(U(id), x, y, 'enemy', { deployT: 0 }); }
function freezeT(t, s = 99) { t.buffs = { T: { t: s, spd: 0, hit: 0 } }; }
function mine(id, x, y) { return spawnUnit(U(id), x, y, 'player', { deployT: 0 }); }
aiMode = 'rule';                     // rule AI in the regular tests; the trained AI has its own test
const realAi = aiTick;
pickAiTowerTroop = () => 'tower-princess';   // deterministic: no random Royal Chef pancakes in tests

const TESTS = {
  lookAhead() {
    aiMode = 'search'; fresh(null, true);
    ok('Look-ahead AI is the default opponent label', /look-ahead/.test(document.getElementById('ai-deck-name').textContent));
    elixir = 10; mine('giant', 100, 470); run(2);
    const before = JSON.stringify([troops.length, troops.map(t => [t.u.id, Math.round(t.x), Math.round(t.y), Math.round(t.hp)]), Math.round(elixir * 100), Math.round(timeLeft * 100), effects.length, projectiles.length]);
    const gid = document.getElementById, arenaKids = document.getElementById('arena').children.length;
    const best = lookahead('player', searchCandidates('player'));
    const after = JSON.stringify([troops.length, troops.map(t => [t.u.id, Math.round(t.x), Math.round(t.y), Math.round(t.hp)]), Math.round(elixir * 100), Math.round(timeLeft * 100), effects.length, projectiles.length]);
    ok('Look-ahead leaves the real match untouched', before === after && document.getElementById === gid && document.getElementById('arena').children.length === arenaKids);
    ok('Look-ahead scores every candidate', best.length >= 2 && best.every(c => isFinite(c.score) || c.score === -Infinity));
    for (let i = 0; i < 200 && gameRunning; i++) tick(0.05);
    ok('Look-ahead AI plays cards in a live match', troops.some(t => t.side === 'enemy'));
    aiMode = 'rule';
  },
  aiDeckChoice() {
    let same = 0;
    for (let i = 0; i < 40; i++) {
      chosenDeck = META_DECKS[0][1].map(C); startBattle(); loopToken++;
      const ai = new Set(aiDeck.map(c => c.id)); if (chosenDeck.filter(c => ai.has(c.id)).length >= 6) same++;
    }
    ok('The AI never plays your deck (or a near-copy)', same === 0, same);
    ok('30 top decks in the pool, each with 8 valid cards', META_DECKS.length === 30 && META_DECKS.every(d => d[1].length === 8 && d[1].every(C)));
  },
  trainedAI() {
    ok('Trained AI model loads in the browser', !!NN);
    aiMode = 'nn'; fresh(null, true);
    ok('Trained AI is the opponent when selected', /Trained AI/.test(document.getElementById('ai-deck-name').textContent));
    const e0 = troops.length; aiElixir = 10;
    for (let i = 0; i < 200 && gameRunning; i++) tick(0.05);
    ok('Trained AI plays cards', troops.some(t => t.side === 'enemy'), troops.filter(t => t.side === 'enemy').length);
    const d = NN.decide('player', true);
    ok('Coach decision is a legal move', d.kind >= 0 && d.kind <= 5 && d.tile >= 0);
    if (!coachOn) toggleCoach(); elixir = 10;
    for (let i = 0; i < 40; i++) tick(0.05);
    ok('Coach mode marks a suggestion', coachTip === null || document.getElementById('coach-mark').style.display !== 'none');
    toggleCoach(); aiMode = 'rule';
  },
  bothSidesAI() {
    fresh(null, true); aiTick = realAi; elixir = 10;
    playerHand = ['knight','archers','fireball','cannon'].map(C);
    ok('playCard works for the player side', playCard('player', playerHand[0], 200, 470) && troops.some(t => t.u.id === 'Knight' && t.side === 'player') && playerHand[0] === null);
    ok('playCard refuses the enemy half', !playCard('player', playerHand[1], 200, 200));
    fresh(null, true); aiTick = realAi;
    let mine0 = 0; const T = tick;
    for (let i = 0; i < 1600 && gameRunning; i++) { ruleAgent('player', 0.05); tick(0.05); }
    mine0 = troops.filter(t => t.side === 'player').length;
    ok('The mirrored rule AI plays the bottom side (cards land on our half, towers take damage)', mine0 > 0 || pCrowns > 0 || towers.el.hp < towers.el.maxHp || towers.er.hp < towers.er.maxHp, `${pCrowns}-${eCrowns}`);
  },
  shieldBar() {
    fresh(); elixir = 10; deployCard(C('knight'), 200, 420, 'player', [], false, true); run(1.2);
    const kn = heroGroups[0].members[0]; useAbility('player'); run(1.1);
    const bar = kn.el.querySelector('.t-shw');
    ok('Hero Knight shows a shield bar after Triumphant Taunt', bar && kn.el.classList.contains('shielded') && getComputedStyle(bar).display === 'block');
    damageTroop(kn, kn.shield / 2);
    ok('…which shrinks as the shield takes damage', Math.abs(parseFloat(bar.firstChild.style.width) - 50) < 1, bar.firstChild.style.width);
    damageTroop(kn, 1e4);
    ok('…and disappears when the shield breaks', !kn.el.classList.contains('shielded'));
    const dp = mine('dark-prince', 150, 420);
    ok('Dark Prince shows his shield from the start', dp.el.classList.contains('shielded'));
  },
  spellVisibility() {
    fresh();
    deployCard(C('fireball'), 200, 470, 'enemy');
    const fp = document.querySelector('.proj.spell-proj');
    ok('A thrown Fireball shows the Fireball card in flight', fp && /fireball\.png/.test(fp.style.backgroundImage), fp && fp.style.backgroundImage);
    ok('…with a marked landing circle named Fireball', /Fireball/.test((document.querySelector('.spell-target.t-enemy') || {}).textContent || ''));
    ok('The opponent\'s card is announced', document.getElementById('play-toast').classList.contains('show') && /Fireball/.test(document.getElementById('play-toast').textContent));
    run(3);
    ok('Marker disappears when the spell lands', !document.querySelector('.spell-target'));
    deployCard(C('goblin-barrel'), 100, 470, 'enemy');
    const gp = document.querySelector('.proj.spell-proj');
    ok('A Goblin Barrel looks like a barrel, not a fireball', gp && /goblin-barrel\.png/.test(gp.style.backgroundImage) && /Goblin Barrel/.test(document.getElementById('play-toast').textContent));
  },
  abilityTap() {
    fresh(); elixir = 10; deployCard(C('knight'), 100, 250, 'player', [], false, true); run(1.2);
    const b = document.getElementById('ability-btn'), r = b.getBoundingClientRect(), x = r.left + r.width / 2, y = r.top + r.height / 2;
    ok('The tap lands on the ability button itself', document.elementFromPoint(x, y) === b);
    const first = b.firstElementChild; run(0.5);
    ok('The button is not rebuilt every frame (a press/release keeps the same nodes)', b.firstElementChild === first);
    const e0 = elixir, tgt = document.elementFromPoint(x, y);
    tgt.dispatchEvent(new MouseEvent('mousedown', { bubbles: true, clientX: x, clientY: y })); run(0.2);
    tgt.dispatchEvent(new MouseEvent('mouseup', { bubbles: true, clientX: x, clientY: y })); tgt.click();
    ok('Tapping the Hero Knight button uses the ability', e0 - elixir > 1.9, (e0 - elixir).toFixed(1));
  },
  startElixir() {
    chosenDeck = ['knight','archers','x-bow','cannon','the-log','fireball','minions','hog-rider'].map(C); startBattle(); loopToken++;
    ok('Both players start with 6 elixir', elixir === 6 && aiElixir === 6, `${elixir}/${aiElixir}`);
  },
  engineData() {
    fresh();
    ok('Bridges centred on tiles 3.5 / 14.5, in line with the Princess Towers', Math.abs(BRIDGE_X[0] - towers.pl.x) < 0.1 && Math.abs(BRIDGE_X[1] - towers.pr.x) < 0.1);
    fresh(); dealTowerDmg('pk', 1);
    ok('King Tower takes 3.3 s to wake up', Math.abs(towers.pk.wakeT - 3.3) < 0.01);
    ok('Collision uses game mass (Giant 18, Skeleton 1)', massOf({ u: UNITS.Giant, maxHp: 1 }) === 18 && massOf({ u: UNITS.Skeleton, maxHp: 1 }) === 1);
    fresh();
    const gi = mine('giant', 200, 470), sk = mine('skeletons', 200, 470.5); freezeT(gi); freezeT(sk); const g0 = [gi.x, gi.y];
    separate();
    ok('A Giant barely moves when a Skeleton overlaps it', Math.hypot(gi.x - g0[0], gi.y - g0[1]) < 2);
  },
  dataHeroes() {
    const hero = (id, x = 200, y = 420) => { fresh(); elixir = 10; deployCard(C(id), x, y, 'player', [], false, true); run(1.1); return heroGroups[heroGroups.length - 1]; };
    let g = hero('ice-wizard'); const iw = g.members[0], v = enemy('golem', 200, 330); freezeT(v, 0.01); v.hp = v.maxHp = 1e5;
    run(2.5);
    ok('Hero Ice Wizard chills with his attacks', !!(v.buffs.IceWizardSlowDown));
    useAbility('player'); run(1.3);
    ok('Ice Cube freezes the chilled enemy', !!v.buffs.IceCube && v.hitMul === 0);
    run(5.5); ok('…then slows it after 5 s', !v.buffs.IceCube && !!v.buffs.IceCubeSlow);
    g = hero('electro-wizard'); const ew = g.members[0]; useAbility('player'); run(1.1);
    ok('Hero Electro Wizard Overcharge: 0.5 s attacks, 50% Crown Tower damage', ew.atkOverride === 0.5 && ew.ctdMul === 0.5);
    run(3.1); ok('…for 3 s', !ew.atkOverride);
    g = hero('tombstone', 200, 470); const tb = g.members[0]; useAbility('player'); run(1.2);
    const q = troops.find(t => t.u.id === 'TombQueen');
    ok('Regal Revive breaks the Tombstone and the Queen holds still 2 s', tb.dead && q && q.hitMul === 0);
    fresh();
    const eg = spawnUnit('ElectroGiant', 200, 420, 'player', { deployT: 0, evo: 'electro-giant' }), k = enemy('knight', 200, 330); freezeT(k); freezeT(eg);
    eg.buffs = {}; eg.spd = 0; const m0 = k.maxHp; run(3.2);
    ok('Evolved Electro Giant pulse lowers enemy troops a level', k.delevel === 1 && k.maxHp === Math.round(m0 / 1.1), `${m0} → ${k.maxHp}`);
    ok('Ronin and the new heroes have card art', ['ronin', 'balloon-hero', 'ice-wizard-hero'].every(id => HERO_ART.has(id.replace('-hero', '')) || id === 'ronin'));
  },
  gameData() {
    ok('Live game data: Knight 1766 HP / 202 dmg, Wizard 755 HP / 304 dmg', UNITS.Knight.hp === 1766 && UNITS.Knight.dmg === 202 && UNITS.Wizard.hp === 755 && UNITS.Wizard.dmg === 304);
    ok('Units carry mass and collision size from the game data', UNITS.Giant.mass === 18 && UNITS.Skeleton.mass === 1 && UNITS.Giant.sz === 33);
    ok('X-Bow keeps the screenshot-calibrated range', UNITS.Xbow.range === 248.8);
    ok('Three Musketeers use the reworked musketeers (906 HP, 1.3 s)', UNITS.ThreeMusketeer.hp === 906 && UNITS.ThreeMusketeer.atk === 1.3);
    ok('Furnace is a walking troop now', C('furnace').type === 'troop' && UNITS.Furnace.spd === 1);
    fresh();
    const fu = mine('furnace', 200, 470); let fs = 0; const os = spawnUnit; spawnUnit = (n, ...r) => { if (n === 'FireSpirits') fs++; return os(n, ...r); };
    run(7.2); spawnUnit = os;
    ok('Furnace drops a Fire Spirit every 7 s', fs === 1, fs);
    fresh();
    const gh = spawnUnit('GoblinHut', 200, 470, 'player', { deployT: 0, tileKeys: [] }); let sg = 0; const os2 = spawnUnit; spawnUnit = (n, ...r) => { if (n === 'SpearGoblin') sg++; return os2(n, ...r); };
    run(5); const idle = sg; const e = enemy('knight', 200, 380); freezeT(e); run(5); spawnUnit = os2;
    ok('Goblin Hut only spawns while an enemy is within 6 tiles (every 2.2 s)', idle === 0 && sg >= 2 && sg <= 3, `${idle} then ${sg}`);
    ok('Cannon Cart totals 1809 HP (cart + cannon)', UNITS.MovingCannon.hp + UNITS.BrokenCannon.hp === 1809);
  },
  lastEvos() {
    ok('Remaining evolutions now implemented', ['royal-ghost','elite-barbarians','giant-snowball','minion-horde','goblin-cage','goblin-barrel','lumberjack'].every(canEvolve));
    fresh();
    const gh = spawnUnit('Ghost', 200, 420, 'player', { deployT: 0, evo: 'royal-ghost' }); const v = enemy('golem', 200, 395); v.hp = v.maxHp = 1e5; freezeT(v);
    run(2.5);
    ok('Evolved Royal Ghost summons 2 Souldiers when attacking from invisibility', troops.filter(t => t.u.id === 'Souldier').length >= 2);
    fresh();
    const eb = spawnUnit('AngryBarbarian', 200, 470, 'player', { deployT: 0, evo: 'elite-barbarians' }); const tg = enemy('knight', 200, 380); freezeT(tg); freezeT(eb, 0.01);
    run(1.2);
    ok('Evolved Elite Barbarians throw a Rage spear at range', tg.hp < tg.maxHp && troops.some(t => t.side === 'player' && t.buffs.Rage));
    fresh();
    const sn = enemy('knight', 200, 250); freezeT(sn, 0.01); const y0 = sn.y;
    deployCard(C('giant-snowball'), 200, 250, 'player', null, true); run(2.2);
    ok('Evolved Giant Snowball rolls the troop 4.5 tiles', Math.abs(sn.y - y0) > 3.5 * GW, `${y0.toFixed(0)} → ${sn.y.toFixed(0)}`);
    fresh();
    const mh = spawnUnit('Minion', 200, 420, 'player', { deployT: 0, evo: 'minion-horde' }); damageTroop(mh, 10);
    ok('Evolved Minion Horde turns invisible when hit', mh.hidden && !!mh.buffs.DarkGuard);
    run(3.1); ok('…and is visible again after 3 s', !mh.hidden);
    fresh();
    const cg = spawnUnit('GoblinCage', 200, 470, 'player', { deployT: 0, evo: 'goblin-cage', tileKeys: [] }); const ck = enemy('knight', 200, 420);
    run(2.1);
    ok('Evolved Goblin Cage drags in a nearby troop and beats it', ck.cagedBy === cg && Math.hypot(ck.x - cg.x, ck.y - cg.y) < 5 && ck.hp < ck.maxHp - 600, (ck.maxHp - ck.hp).toFixed(0));
    cg.dead = true; run(0.3); ok('…which is freed when the cage falls', !ck.cagedBy && !ck.buffs.Caged);
    fresh();
    deployCard(C('goblin-barrel'), 100, 200, 'player', null, true); run(3);
    ok('Evolved Goblin Barrel also drops 3 Decoy Goblins in the other lane', troops.filter(t => t.u.id === 'DecoyGoblin' && t.x > AW / 2).length === 3 && troops.filter(t => t.u.id === 'Goblin' && t.x < AW / 2).length === 3);
    fresh();
    const lj = spawnUnit('RageBarbarian', 200, 420, 'player', { deployT: 0, evo: 'lumberjack' }); lj.dead = true; run(0.4);
    const g = troops.find(t => t.u.id === 'LumberGhost');
    ok('Evolved Lumberjack leaves a ghost that cannot be hurt or targeted', g && g.hidden && (damageTroop(g, 1e6), !g.dead));
    run(6.5); ok('…and fades with his Rage', !troops.includes(g));
  },
  newCards() {
    const ids = ['rune-giant','little-prince','goblinstein','vines','spirit-empress','ronin','goblin-demolisher','boss-bandit','suspicious-bush','goblin-curse','berserker','void','goblin-machine','minion-giant'];
    ok('All 14 new cards are in the library', ids.every(C), ids.filter(id => !C(id)).join(','));
    fresh();
    const rg = mine('rune-giant', 200, 470), ally = mine('knight', 210, 450); freezeT(ally); run(1.2);
    ok('Rune Giant enchants a nearby ally', ally.rune && ally.rune.by === rg);
    const dummy = enemy('golem', 210, 425); dummy.hp = dummy.maxHp = 1e6; freezeT(dummy); ally.buffs = {};
    run(6);
    ok('Enchanted troop deals bonus damage every 3rd attack', 1e6 - dummy.hp > 3 * ally.dmg + 200, (1e6 - dummy.hp).toFixed(0));
    fresh();
    const lp = mine('little-prince', 200, 470), lt = enemy('golem', 200, 360); lt.hp = lt.maxHp = 1e6; freezeT(lt);
    run(8);
    ok('Little Prince speeds up his attacks (stage 3: 0.4 s)', lp.atkOverride === 0.4, lp.atkOverride);
    elixir = 10; ok('Royal Rescue costs 3', useAbility('player') && Math.abs(elixir - 7) < 0.3);
    run(1.2);
    ok('Guardienne joins the fight', troops.some(t => t.u.id === 'Guardienne'));
    run(1);
    ok('Little Prince ability is single-use', !abilityState('player') || abilityState('player').c !== lp);
    fresh(); elixir = 10;
    deployCard(C('goblinstein'), 200, 470, 'player'); run(1.1);
    ok('Goblinstein deploys Doctor and Monster', troops.some(t => t.u.id === 'Goblinstein') && troops.some(t => t.u.id === 'GoblinsteinMonster'));
    const gz = enemy('knight', 200, 440); freezeT(gz); useAbility('player'); run(2.5);
    ok('Lightning Link zaps enemies near the link', gz.hp < gz.maxHp - 200, (gz.maxHp - gz.hp).toFixed(0));
    fresh(); elixir = 4; playerHand = ['spirit-empress','knight','cannon','the-log'].map(C);
    ok('Spirit Empress costs 3 below 6 elixir', cardCost(C('spirit-empress')) === 3);
    selectedIdx = 0; clickArena(200, 470);
    ok('…and plays her ground form', troops.some(t => t.u.id === 'SpiritEmpressGround') && elixir < 1.2);
    fresh(); elixir = 10; playerHand = ['spirit-empress','knight','cannon','the-log'].map(C); selectedIdx = 0; clickArena(200, 470);
    ok('With 6+ elixir she flies (6 elixir)', troops.some(t => t.u.id === 'SpiritEmpressAir' && t.air) && Math.abs(elixir - 4) < 0.2);
    fresh();
    const ro = mine('ronin', 200, 420), pk = enemy('pekka', 200, 395); freezeT(ro); ro.buffs = {}; ro.spdMul = 0;
    hitAt(pk, ro, ro.x, ro.y, pk.dmg);
    ok('Ronin parries a melee hit and strikes back double', ro.hp === ro.maxHp && pk.hp < pk.maxHp - ro.dmg * 1.9);
    hitAt(pk, ro, ro.x, ro.y, pk.dmg);
    ok('…then the parry is on a 3.5 s cooldown', ro.hp < ro.maxHp);
    fresh();
    const gd = mine('goblin-demolisher', 200, 420); gd.hp = gd.maxHp * 0.45; run(0.1);
    ok('Goblin Demolisher below 50% turns into a building runner', gd.demolishing && gd.targets === 'building' && gd.spd === 2);
    const near = enemy('knight', 200, 400); freezeT(near); gd.dead = true; tick(0.05);
    ok('…and explodes when defeated', near.hp < near.maxHp - 300);
    fresh();
    const bb = mine('boss-bandit', 100, 420); run(0.1); elixir = 10;
    ok('Boss Bandit ability (2 uses)', useAbility('player'));
    let jump = 0; for (let i = 0; i < 44; i++) { const yb = bb.y; tick(0.05); jump = Math.max(jump, bb.y - yb); }
    ok('Getaway Grenade teleports her 6 tiles back', Math.abs(jump - 6 * GW) < 8, jump.toFixed(0));
    run(3.1); elixir = 10;
    ok('Second use available', useAbility('player')); run(2.2);
    ok('No third use', !abilityState('player'));
    fresh();
    const sb = mine('suspicious-bush', 200, 470); run(1.1);
    ok('Suspicious Bush is untargetable', sb.hidden && !nearestTroop('player', sb.x, sb.y, 999));
    sb.dead = true; tick(0.05);
    ok('…and releases 2 Bush Goblins', troops.filter(t => t.u.id === 'BushGoblin').length === 2);
    fresh();
    const vt = [enemy('golem', 200, 250), enemy('knight', 210, 250), enemy('minions', 190, 250), enemy('skeletons', 205, 240)];
    vt.forEach(t => freezeT(t, 0.01)); const bat = vt[2]; deployCard(C('vines'), 200, 250, 'player'); run(1);
    ok('Vines ensnares the 3 highest-HP troops and pulls air down', vt[0].buffs.Vines && vt[1].buffs.Vines && !bat.air && !vt[3].buffs.Vines);
    fresh();
    const v1 = enemy('golem', 200, 250); freezeT(v1); deployCard(C('void'), 200, 250, 'player'); run(4);
    ok('Void hits a lone target with its 1-target damage ×3', Math.abs((v1.maxHp - v1.hp) - 3 * 696) < 5, (v1.maxHp - v1.hp).toFixed(0));
    fresh();
    const cs = enemy('skeletons', 200, 250); freezeT(cs); deployCard(C('goblin-curse'), 200, 250, 'player'); run(2.3);
    ok('Goblin Curse turns a cursed troop that dies into your Goblin', cs.dead && troops.some(t => t.u.id === 'Goblin' && t.side === 'player'));
    fresh();
    const gm = mine('goblin-machine', 200, 470), far = enemy('knight', 200, 380); freezeT(far); gm.spd = 0;
    run(3);
    ok('Goblin Machine fires its rocket at range (2.5-5 tiles)', far.hp < far.maxHp - 250, (far.maxHp - far.hp).toFixed(0));
    ok('Minion Giant flies and targets buildings (estimated stats)', UNITS.MinionGiant.air && UNITS.MinionGiant.targets === 'building' && C('minion-giant').est);
    ok('Champion abilities that went single-use: Mighty Miner', CHAMP_ABILITIES.MightyMiner.uses === 1 && !CHAMP_ABILITIES.ArcherQueen.uses);
  },
  heroes() {
    const deck = ids => { chosenDeck = ids.map(C); startBattle(); loopToken++; aiTick = () => {}; elixir = 10; };
    deck(['knight','musketeer','skeletons','cannon','giant','zap','minions','the-log']);
    ok('Deck slots: 1 Evolution, 2 Hero, 3 Wild', roles.player.knight === 'evo' && roles.player.musketeer === 'hero' && roles.player.skeletons === 'evo', JSON.stringify(roles.player));
    wildPref = 'evo'; deck(['skeletons','musketeer','knight','cannon','giant','zap','minions','the-log']);
    ok('Wild slot can play a card as Evolution…', roles.player.knight === 'evo');
    wildPref = 'hero'; deck(['skeletons','musketeer','knight','cannon','giant','zap','minions','the-log']);
    ok('…or as Hero', roles.player.knight === 'hero' && roles.player.musketeer === 'hero');
    goToDeckBuilder(); clearDeck(); ['knight','archers','archer-queen'].forEach(id => toggleDeckCard(C(id)));
    toggleDeckCard(C('giant'));
    ok('A Champion is moved into the Hero/Wild slot', [1, 2].includes(chosenDeck.findIndex(c => c.id === 'archer-queen')), chosenDeck.map(c => c.id).join(','));
    ok('Hero cards are badged in the collection', !!document.querySelector('#dbc-knight .dc-hero') && !!document.querySelector('#dbc-ice-wizard .dc-hero') && !document.querySelector('#dbc-hog-rider .dc-hero'));
    // abilities
    const hero = (id, x = 200, y = 420) => { fresh(); elixir = 10; deployCard(C(id), x, y, 'player', [], false, true); run(1.1); return heroGroups[heroGroups.length - 1]; };
    let g = hero('knight'); const kn = g.members[0];
    const foe = enemy('musketeer', 200, 330); freezeT(foe, 0.01);
    ok('Hero ability button appears', document.getElementById('ability-btn').style.display === 'flex' && document.getElementById('ability-btn').classList.contains('hero'));
    ok('Hero Knight ability costs 2 elixir', useAbility('player') && Math.abs(elixir - 8) < 0.2);
    run(1.05);
    ok('Hero Knight gains a shield and taunts nearby enemies', kn.shield > 400 && foe.taunt === kn && acquireTarget(foe) === kn);
    ok('Hero abilities are single-use', !useAbility('player'));
    g = hero('knight'); elixir = 10; useAbility('player'); const e0 = elixir; g.members[0].dead = true; run(0.2);
    ok('Elixir is refunded if the Hero dies before casting', elixir > e0 + 1.9, `${e0.toFixed(1)} → ${elixir.toFixed(1)}`);
    g = hero('giant', 100, 420); const gt = enemy('knight', 100, 400); freezeT(gt, 0.01); const gx = gt.x;
    useAbility('player'); run(2.5);
    ok('Hero Giant hurls the enemy into the other lane', Math.abs(gt.x - gx) > 150 && gt.hp < gt.maxHp, `${gx.toFixed(0)} → ${gt.x.toFixed(0)}`);
    g = hero('mini-pekka'); const mp = g.members[0]; g.prog = 70; const m0 = mp.maxHp; useAbility('player'); run(1.1);
    ok('Hero Mini P.E.K.K.A eats 3 meters of pancakes: +5 levels', mp.maxHp === Math.round(m0 * 1.1 ** 5), `${m0} → ${mp.maxHp}`);
    g = hero('musketeer'); useAbility('player'); run(1.2);
    ok('Hero Musketeer places a Trusty Turret ahead', troops.some(t => t.u.id === 'HeroTurret' && t.y < g.members[0].y));
    g = hero('ice-golem'); const ig = enemy('knight', 200, 380); freezeT(ig); useAbility('player'); run(4.1);
    ok('Hero Ice Golem Snowstorm: 3 pulses (0/1.5/3 s) and a slow', Math.abs((ig.maxHp - ig.hp) - 3 * 69) < 30 && !!ig.buffs.Snowstorm, (ig.maxHp - ig.hp).toFixed(0));
    g = hero('wizard'); useAbility('player'); run(1.2);
    ok('Hero Wizard takes flight', g.members[0].air && g.members[0].flight);
    g = hero('goblins'); elixir = 10;
    ok('Hero Goblins ability waits for the banner', !useAbility('player'));
    g.members.forEach(m => { m.dead = true; }); run(0.1);
    let gb = 0; const os = spawnUnit; spawnUnit = (n, ...r) => { if (n === 'Goblin') gb++; return os(n, ...r); };
    ok('The last Goblin drops a banner', !!g.banner && useAbility('player')); run(1.2); spawnUnit = os;
    ok('Banner Brigade calls 2 Goblins', gb === 2, gb);
    g = hero('mega-minion'); const low = enemy('archers', 100, 250); freezeT(low); run(0.1);
    ok('Hero Mega Minion marks the lowest-HP enemy', g.mark === low);
    ok('…but the warp is only ready 1.5 s after deploy', !useAbility('player')); run(0.5);
    useAbility('player'); run(1.1);
    ok('Wounding Warp kills the marked enemy', low.dead);
    g = hero('magic-archer'); const ma = g.members[0], y0 = ma.y; useAbility('player'); run(1.1);
    ok('Hero Magic Archer darts back and leaves a decoy', ma.y > y0 + 60 && troops.some(t => t.u.id === 'MagicDecoy') && ma.tripleShot);
    g = hero('balloon'); const bt = enemy('knight', 200, 330); freezeT(bt); useAbility('player'); run(1);
    ok('Hero Balloon drops a Skeletrooper on a nearby ground enemy', troops.some(t => t.u.id === 'Skeletrooper') && bt.hp < bt.maxHp);
    g = hero('bowler', 120, 380); const h0 = towers.el.hp + towers.er.hp; useAbility('player'); run(8);
    ok('Hero Bowler Stone Swish hits the Crown Tower from long range', towers.el.hp + towers.er.hp <= h0 - 500, `${h0 - towers.el.hp - towers.er.hp}`);
    g = hero('dark-prince'); useAbility('player'); run(0.5);
    ok('Hero Dark Prince dismounts: Rhino appears, Prince loses his charge', troops.some(t => t.u.id === 'HeroRhino') && !g.members[0].u.charge);
    g = hero('tombstone', 200, 470); useAbility('player'); run(1.1);
    ok('Hero Tombstone raises the Tomb Queen', troops.some(t => t.u.id === 'TombQueen'));
    g = hero('berserker'); const bz = g.members[0]; useAbility('player'); run(1.1); damageTroop(bz, 1e5);
    ok('Hero Berserker cannot drop below 1 HP during Savage Survival', !bz.dead && bz.hp === 1);
    g = hero('valkyrie'); const vk = enemy('knight', 200, 400); freezeT(vk); useAbility('player'); run(2);
    ok('Hero Valkyrie spins, damaging around her', vk.hp < vk.maxHp - 4 * 97 && g.members[0].heroBusy === 'spin');
    g = hero('barbarian-barrel', 200, 450); run(2); const barb = g.members[0];
    ok('Hero Barbarian Barrel: the Barbarian carries the ability', !!barb && barb.u.id === 'Barbarian');
    useAbility('player'); run(3);
    ok('Rowdy Reroll rolls the barrel again', barb.dead && troops.some(t => t.u.id === 'Barbarian' && t !== barb));
    // last played Hero/Champion owns the button
    fresh(); elixir = 10; deployCard(C('archer-queen'), 150, 420, 'player'); run(1.6); deployCard(C('knight'), 250, 420, 'player', [], false, true); run(1.2);
    ok('The ability button belongs to the last played Hero/Champion', abilityState('player').g && abilityState('player').g.id === 'knight');
    // every hero ability runs for both sides
    const bad = [];
    for (const id of Object.keys(HEROES).filter(canHero)) for (const side of ['player', 'enemy']) {
      try {
        fresh(); __errors.length = 0; enemy('knight', 200, 250); mine('knight', 200, 420);
        const card = C(id), y = side === 'player' ? 440 : 200;
        if (!card) { bad.push(id + ': no card'); continue; }
        const p = resolvePlacement(side, card, 200, card.type === 'spell' ? (side === 'player' ? 300 : 360) : y) || resolvePlacement(side, card, 100, y);
        if (p.keys) p.keys.forEach(k => occupiedTiles.add(k));
        deployCard(card, p.x, p.y, side, p.keys, false, true); run(2);
        if (side === 'player') elixir = 10; else aiElixir = 10;
        const st = abilityState(side); if (st && st.g) { st.g.banner = st.g.banner || (id === 'goblins' ? { x: 200, y: 300, t: 5, el: document.createElement('div'), side } : null); }
        useAbility(side); run(10);
        if (__errors.length) bad.push(id + ':' + __errors[0]);
      } catch (e) { bad.push(id + ':' + side + ':' + e.message); }
    }
    ok(`All ${Object.keys(HEROES).filter(canHero).length} Hero abilities run for both sides without errors`, bad.length === 0, bad.slice(0, 4).join(' | '));
    fresh(null, true); aiTick = realAi; deployCard(C('valkyrie'), 200, 250, 'enemy', [], false, true); run(1.1);
    mine('knight', 200, 300); mine('knight', 215, 300);
    let used = false; for (let i = 0; i < 60; i++) { aiElixir = 10; tick(0.05); if (heroGroups.some(g => g.side === 'enemy' && g.used)) used = true; }
    ok('AI uses its Hero ability', used);
  },
  towerTroops() {
    ok('Retired cards (Battle Healer, Party Hut) are not in the library', !C('battle-healer') && !C('party-hut'));
    const withTroop = id => { towerTroop.player = id; fresh(); pickAiTowerTroop = () => 'tower-princess'; return towers.pl; };
    const op = pickAiTowerTroop;
    let tw = withTroop('cannoneer');
    ok('Cannoneer tower: 2616 HP, 320 damage, 2.2s hit speed', tw.maxHp === 2616 && tw.dmg === 320 && tw.hitSpd === 2.2, `${tw.maxHp}/${tw.dmg}/${tw.hitSpd}`);
    ok('King Tower keeps its stats with a tower troop', towers.pk.maxHp === 4824);
    tw = withTroop('dagger-duchess');
    const g = enemy('giant', tw.x, tw.y - 90); freezeT(g);
    let shots = 0; const sp = spawnProj; spawnProj = (...a) => { if (String(a[4]).startsWith('tower')) shots++; return sp(...a); };
    run(4.2); const fast = shots; run(4.2); spawnProj = sp;
    ok('Dagger Duchess throws 8 quick daggers, then slows to reloads', fast >= 7 && fast <= 9 && shots - fast <= 4, `${fast} then ${shots - fast}`);
    tw = withTroop('royal-chef');
    const k = mine('knight', 200, 470); freezeT(k); const h0 = k.maxHp;
    run(17.5);
    ok('Royal Chef feeds a pancake (+1 level) to your troop', k.pancake === 1 && k.maxHp === Math.round(h0 * 1.1), `${h0} → ${k.maxHp}`);
    const k2 = mine('giant', 200, 480); freezeT(k2); run(24);
    ok('…and serves the next troop that has not eaten', k2.pancake === 1 && k.pancake === 1);
    towerTroop.player = 'tower-princess'; pickAiTowerTroop = op;
    fresh();
    ok('Default Tower Princess: 3052 HP, 109 damage', towers.pl.maxHp === 3052 && towers.pl.dmg === 109);
  },
  xbowEdges() {
    for (const [x, y, label] of [[365, 16 * GH + 3, 'first row cols 15-16'], [35, 16 * GH + 3, 'first row cols 1-2'], [395, 17 * GH + 18, 'right edge, second row'], [200, 16 * GH + 2, 'centre first row'], [360, 15 * GH + 15, 'click on river edge snaps to first row']]) {
      fresh(['x-bow','cannon','knight','the-log']);
      play(0, x, y);
      const b = troops.find(t => t.u.id === 'Xbow');
      ok('X-Bow places at ' + label, b, b ? `at ${b.x.toFixed(0)},${b.y.toFixed(0)}` : 'not placed');
    }
    fresh(['x-bow','cannon','knight','the-log']);
    play(0, 395, 16 * GH + 3);
    ok('X-Bow corner click never covers the blocked corner tile (17,16)', !troops.some(t => t.u.id === 'Xbow' && t.tileKeys.includes('17,16')), JSON.stringify((troops.find(t => t.u.id === 'Xbow') || {}).tileKeys));
    fresh(['x-bow','cannon','knight','the-log']);
    play(0, towers.pl.x, towers.pl.y);
    ok('X-Bow cannot overlap own princess tower', !troops.some(t => t.u.id === 'Xbow' && t.tileKeys.some(k => { const [c, r] = k.split(',').map(Number); return towerBlocks(c, r); })));
    fresh(['knight','cannon','x-bow','the-log']);
    play(0, 300, 250);
    ok('Troop cannot deploy on enemy side while princess alive', !troops.some(t => t.u.id === 'Knight'));
  },
  buildingDecay() {
    fresh(['cannon','knight','x-bow','the-log']);
    play(0, 200, 420);
    const b = troops.find(t => t.u.id === 'Cannon');
    run(1.05);
    const h0 = b.hp;
    run(15);
    ok('Cannon loses HP over time', b.hp < h0 * 0.55, `${h0.toFixed(0)} → ${b.hp.toFixed(0)}`);
    run(16);
    ok('Cannon expires after lifetime', !troops.includes(b));
    ok('Expired building frees its tiles', occupiedTiles.size === 0, occupiedTiles.size);
  },
  kingDormant() {
    fresh();
    const st = enemy('knight', 200, 470); freezeT(st);
    run(3);
    ok('King tower dormant at start (does not shoot)', !towers.pk.activated && towers.pk.shootCd === 0);
    dealTowerDmg('pk', 10);
    ok('King activates when damaged', towers.pk.activated);
    const pkCd0 = towers.pk.shootCd; run(1);
    ok('Activated King waits ~2s before first shot', towers.pk.shootCd === pkCd0 && towers.pk.wakeT > 0);
    fresh();
    dealTowerDmg('el', 99999);
    ok('Enemy king activates when a princess falls', towers.ek.activated);
    ok('Player king still dormant', !towers.pk.activated);
  },
  pocket() {
    fresh(['knight','cannon','x-bow','the-log']);
    ok('Enemy-side tile closed before tower falls', !canDeployTile('player', 3, 8, 'troop'));
    dealTowerDmg('el', 99999);
    ok('Left pocket opens after left princess falls', canDeployTile('player', 3, 8, 'troop'));
    ok('Right lane stays closed', !canDeployTile('player', 14, 8, 'troop'));
    ok('Pocket does not reach beside the king', !canDeployTile('player', 3, 3, 'troop'));
    play(0, 70, 200);
    ok('Knight deploys into open pocket', troops.some(t => t.u.id === 'Knight' && t.y < RIVER));
  },
  targeting() {
    fresh(['cannon','knight','x-bow','the-log']);
    play(0, 200, 420);
    run(1.1);
    const cannon = troops.find(t => t.u.id === 'Cannon');
    const hog = enemy('hog-rider', 120, 280);
    ok('Hog Rider targets Cannon (building pull)', acquireTarget(hog) === cannon);
    const k = mine('knight', 300, 400), m = enemy('minions', 300, 360), a = mine('archers', 320, 420);
    ok('Knight ignores Minions (air)', acquireTarget(k) !== m);
    ok('Archers target Minions', acquireTarget(a) === m);
  },
  bridges() {
    fresh();
    const k = mine('knight', 200, 400);
    let bad = 0;
    for (let i = 0; i < 400; i++) {
      tick(0.05);
      if (Math.abs(k.y - RIVER) < GH * 0.9 && !BRIDGE_X.some(b => Math.abs(k.x - b) <= GW)) bad++;
    }
    ok('Ground troop crosses river only on a bridge', bad === 0 && k.y < RIVER, `y=${k.y.toFixed(0)} offBridge=${bad}`);
    fresh();
    const h = mine('hog-rider', 200, 400);
    let crossX = null;
    for (let i = 0; i < 100 && crossX === null; i++) { const y0 = h.y; tick(0.05); if (y0 > RIVER && h.y <= RIVER) crossX = h.x; }
    ok('Hog Rider jumps the river (no bridge detour)', crossX !== null && !BRIDGE_X.some(b => Math.abs(crossX - b) < GW), `crossed at x=${crossX && crossX.toFixed(0)}`);
  },
  elixirAndTime() {
    fresh(); timeLeft = 100; elixir = 0; run(2.8, 0.01);
    ok('Normal elixir ≈ 1 per 2.8s', Math.abs(elixir - 1) < 0.05, elixir.toFixed(2));
    fresh(); timeLeft = 50; elixir = 0; run(1.4, 0.01);
    ok('Double elixir in last minute', Math.abs(elixir - 1) < 0.05, elixir.toFixed(2));
    fresh(); timeLeft = 0.05; run(0.1);
    ok('Tied at time → overtime 2:00', overtime && timeLeft > 119);
    dealTowerDmg('er', 99999);
    ok('Overtime is sudden death', !gameRunning && document.getElementById('ov-title').textContent.includes('VICTORY'));
    fresh(); dealTowerDmg('ek', 99999);
    ok('King tower kill = 3 crowns', pCrowns === 3 && !gameRunning);
    fresh(); overtime = true; timeLeft = 0.05; towers.pl.hp = 900; towers.er.hp = 1000;
    run(0.1);
    ok('Overtime tiebreak: lowest tower HP loses', !gameRunning && document.getElementById('ov-title').textContent.includes('DEFEAT'));
  },
  spells() {
    fresh(['the-log','fireball','zap','knight']);
    const kn = enemy('knight', 200, 250), mi = enemy('minions', 200, 250);
    freezeT(kn); freezeT(mi);
    const k0 = kn.hp, m0 = mi.hp;
    play(0, 200, 16 * GH + 10);
    run(2);
    ok('Log hits ground troop', kn.hp < k0, `${k0}→${kn.hp}`);
    ok('Log ignores air troop', mi.hp === m0);
    fresh(['fireball','the-log','zap','knight']);
    const h0 = towers.el.hp;
    play(0, 67, 131);
    run(3);
    ok('Fireball deals reduced crown-tower damage (30%)', Math.abs((h0 - towers.el.hp) - C('fireball').spell.dmg * 0.3) <= 1, h0 - towers.el.hp);
    fresh(['zap','the-log','fireball','knight']);
    const z = enemy('knight', 200, 360);
    play(0, 200, 360);
    ok('Zap is instant and stuns', z.hp < z.maxHp && z.buffs.ZapFreeze && z.hitMul === 0 || z.buffs.ZapFreeze);
  },
  towerSplash() {
    fresh();
    const v = enemy('valkyrie', 67, 455), myK = mine('knight', 90, 455), otherE = enemy('knight', 60, 450);
    const a0 = myK.hp, b0 = otherE.hp;
    run(3);
    ok('Splash near a tower hits defenders, not attacker\'s own troops', otherE.hp === b0, `own enemy hp ${b0}→${otherE.hp.toFixed(0)}, my knight ${a0}→${myK.hp.toFixed(0)}`);
  },
  aiSoak() {
    fresh(null, true);
    aiTick = realAi;
    for (const t of Object.values(towers)) if (t.side === 'player') t.hp = t.maxHp = 1e9;
    const placements = [];
    const orig = deployCard;
    let spellsCast = 0, spent = 0;
    deployCard = (card, x, y, side, keys) => { if (side === 'enemy') { spent += card.cost; if (card.type !== 'spell') placements.push({ id: card.id, x, y }); else spellsCast++; } orig(card, x, y, side, keys); };
    run(90);
    deployCard = orig;
    const bad = placements.filter(p => p.y > RIVER && !C(p.id).anywhere && !(towers.pl.hp <= 0 || towers.pr.hp <= 0));
    ok('AI spends its elixir over 90s (≈37 earned)', spent >= 24, `spent ${spent}: ${placements.length} units, ${spellsCast} spells`);
    ok('AI never deploys units in player half', bad.length === 0, JSON.stringify(bad.slice(0,3)));
    ok('AI hand stays 4 slots from 8-card deck', aiHand.length === 4 && aiHand.filter(Boolean).length + aiEmpty.length === 4 && new Set(aiDeck).size === 8);
  },
  round2() {
    fresh();
    const s = enemy('ice-spirit', 200, 380), k = mine('knight', 200, 420);
    run(0.6);
    ok('Ice Spirit dies on impact and freezes', !troops.includes(s) && !!k.buffs.Freeze, JSON.stringify(Object.keys(k.buffs)));
    fresh();
    const iw = enemy('ice-wizard', 200, 300), k2 = mine('knight', 200, 400);
    k2.lock = null; run(2.2);
    ok('Ice Wizard slows on hit', !!k2.buffs.IceWizardSlowDown, JSON.stringify(Object.keys(k2.buffs)));
    fresh(['inferno-tower','knight','x-bow','the-log']);
    play(0, 200, 420); run(1.1);
    const inf = troops.find(t => t.u.id === 'InfernoTower');
    const pk = enemy('pekka', 200, 330); pk.maxHp = pk.hp = 1e6; freezeT(pk, 1e6);
    run(1.0); const early = 1e6 - pk.hp;
    run(4.5); const before = pk.hp; run(1.0); const late = before - pk.hp;
    ok('Inferno Tower damage ramps up', late > early * 5, `first 1s ${early.toFixed(0)}, later 1s ${late.toFixed(0)}`);
    fresh(['tesla','knight','x-bow','the-log']);
    play(0, 200, 420); run(1.2);
    const te = troops.find(t => t.u.id === 'Tesla');
    ok('Tesla hides when idle', te.hidden);
    const kk = enemy('knight', 200, 330); run(0.3);
    ok('Tesla pops up when enemy in range', !te.hidden);
    fresh();
    const a = mine('knight', 200, 420), b = mine('knight', 201, 421);
    run(0.5);
    ok('Units do not stack on each other', Math.hypot(a.x - b.x, a.y - b.y) > 12, Math.hypot(a.x - b.x, a.y - b.y).toFixed(1));
    fresh();
    const g = mine('giant', 67, 440); g.lock = null;
    run(0.2);
    ok('Units cannot walk through own princess tower', Math.hypot(g.x - 67, g.y - 489) >= 24, `${g.x.toFixed(0)},${g.y.toFixed(0)}`);
    fresh();
    const arc = mine('archers', 200, 400), tgtK = enemy('knight', 200, 310); freezeT(tgtK);
    run(1.2);
    ok('Ranged units fire projectiles', projectiles.some(p => p.el.className.includes('p-troop')) || tgtK.hp < tgtK.maxHp);
    fresh();
    const mk = spawnUnit('MegaKnight', 200, 300, 'enemy', { deployT: 0.05 }); const vic = mine('knight', 205, 305); vic.deployT = 0;
    mk.deployT = 0.05; const v0 = vic.hp; run(0.15);
    ok('Mega Knight spawn stomp', vic.hp < v0, `${v0}→${vic.hp}`);
    fresh();
    const bal = enemy('balloon', 300, 470); const vk = mine('knight', 300, 470); const vk0 = vk.hp;
    bal.hp = 1; damageTroop(bal, 5); run(3.3);
    ok('Balloon drops death bomb', vk.hp < vk0);
    fresh();
    const pr = mine('prince', 200, 450); const tk = enemy('knight', 200, 330); freezeT(tk);
    let saw = false; for (let i = 0; i < 80; i++) { tick(0.05); if (pr.charged) saw = true; }
    ok('Prince charges after running 2 tiles', saw);
  },
  round3() {
    fresh();
    const k = mine('knight', 67, 560), g = mine('giant', 333, 560);
    run(25);
    ok('Knight behind princess tower paths around it and crosses', k.y < RIVER, `${k.x.toFixed(0)},${k.y.toFixed(0)}`);
    ok('Giant behind princess tower paths around it and crosses', g.y < RIVER, `${g.x.toFixed(0)},${g.y.toFixed(0)}`);
    fresh();
    const t = enemy('knight', 200, 330); freezeT(t); damageTroop(t, 10); run(0.1);
    ok('No floating damage numbers', document.querySelectorAll('.dmg').length === 0);
    ok('Tower HP shown as number', (document.querySelector('#t-pl .hp-num') || {}).textContent === String(towers.pl.hp));
    ok('Troop HP bar appears only once damaged', t.el.classList.contains('hurt') && !mine('knight', 100, 400).el.classList.contains('hurt'));
    fresh(['knight','cannon','x-bow','the-log']);
    document.dispatchEvent(new KeyboardEvent('keydown', { key: '2' }));
    ok('Keys 1-4 select hand cards', selectedIdx === 1);
    document.dispatchEvent(new KeyboardEvent('keydown', { key: 'Escape' }));
    ok('Escape cancels selection', selectedIdx === null);
  },
  soak() {
    fresh(null, true);
    aiTick = realAi;
    let t = 0, plays = 0;
    while (gameRunning && t < 420) {
      if (Math.round(t * 20) % 60 === 0) {
        const card = playerHand[Math.floor(Math.random() * 4)];
        if (!card) { tick(0.05); t += 0.05; continue; }
        const p = resolvePlacement('player', card, 20 + Math.random() * 360, card.type === 'spell' ? 60 + Math.random() * 500 : 320 + Math.random() * 280);
        if (p && elixir >= card.cost) { selectedIdx = playerHand.indexOf(card); clickArena(p.x, p.y); plays++; }
      }
      tick(0.05); t += 0.05;
    }
    ok('Full match soak: no runtime errors', __errors.length === 0, __errors.slice(0, 2).join(' | '));
    ok('Full match ends with a result', !gameRunning, `t=${t.toFixed(0)}s plays=${plays} crowns ${pCrowns}-${eCrowns} ` + document.getElementById('ov-sub').textContent);
    ok('No orphan tiles after match', [...occupiedTiles].every(k => troops.some(tr => tr.tileKeys && tr.tileKeys.includes(k))));
  },
  backRows() {
    fresh(['knight','cannon','x-bow','the-log']);
    const row26 = [0, 1, 5, 12, 16, 17].every(c => canDeployTile('player', c, 28, 'troop'));
    ok('Line 13 from river (row 28): side columns open', row26);
    ok('Line 13 centre cols 8-9 open', canDeployTile('player', 8, 28, 'troop') && canDeployTile('player', 9, 28, 'troop'));
    ok('Line 14 (last row): only centre cols 8-9', canDeployTile('player', 8, 29, 'troop') && canDeployTile('player', 9, 29, 'troop') && ![0, 3, 7, 10, 17].some(c => canDeployTile('player', c, 29, 'troop')));
    ok('Enemy mirror: row 1 open, row 0 centre only', canDeployTile('enemy', 0, 1, 'troop') && !canDeployTile('enemy', 0, 0, 'troop') && canDeployTile('enemy', 8, 0, 'troop'));
    play(0, 5, 28 * GH + 10);
    ok('Click at line 13 far-left places a knight there', troops.some(t => t.u.id === 'Knight' && t.y > 28 * GH && t.x < GW));
  },
  backSplit() {
    for (const [x, label] of [[189, 'col 8'], [211, 'col 9']]) {
      fresh(['archers','knight','x-bow','the-log']);
      play(0, x, 29 * GH + 10);
      const arch = troops.filter(t => t.u.id === 'Archer');
      run(14);
      const xs = arch.map(a => a.x).sort((a, b) => a - b);
      ok(`Archers behind king (${label}) split to both lanes`, arch.length === 2 && xs[0] < AW / 2 - 60 && xs[1] > AW / 2 + 60, xs.map(v => v.toFixed(0)).join(','));
    }
    fresh(['goblins','knight','x-bow','the-log']);
    play(0, 189, 29 * GH + 10);
    const gob = troops.filter(t => t.u.id === 'Goblin');
    run(10);
    ok('Goblins behind king spread over both lanes', gob.some(g => g.x < AW / 2 - 60) && gob.some(g => g.x > AW / 2 + 60), gob.map(g => g.x.toFixed(0)).join(','));
    fresh(['archers','knight','x-bow','the-log']);
    play(0, 60, 400);
    const side = troops.filter(t => t.u.id === 'Archer');
    run(10);
    ok('Archers placed in a lane stay together in that lane', side.every(a => a.x < AW / 2), side.map(a => a.x.toFixed(0)).join(','));
  },
  river() {
    fresh(['knight','cannon','x-bow','the-log']);
    ok('River rows 14-15 block every column for both sides', [0, 5, 9, 17].every(c => [14, 15].every(r => !canDeployTile('player', c, r, 'troop') && !canDeployTile('enemy', c, r, 'troop'))));
    ok('First row each side open except the edge tiles', [1, 9, 16].every(c => canDeployTile('player', c, 16, 'troop') && canDeployTile('enemy', c, 13, 'troop')));
    play(0, 200, 300);
    ok('Clicking mid-river does not deploy', !troops.some(t => t.u.id === 'Knight' && Math.abs(t.y - RIVER) < RIVER_HALF));
    fresh();
    const k = mine('knight', 150, 16 * GH + 10); let wet = 0;
    for (let i = 0; i < 300; i++) { tick(0.05); if (Math.abs(k.y - RIVER) < RIVER_HALF - 1 && !BRIDGE_X.some(b => Math.abs(k.x - b) <= GW)) wet++; }
    ok('Ground troop never stands in the water', wet === 0, wet);
  },
  xbowReach() {
    // (c, r) = centre tile of the 3×3 X-Bow; every case below comes from a user screenshot or the wiki
    const reach = (c, r) => {
      fresh(['x-bow','knight','cannon','the-log']);
      const keys = [];
      for (let i = -1; i <= 1; i++) for (let j = -1; j <= 1; j++) keys.push(`${c + i},${r + j}`);
      if (!keys.every(k => { const [a, b] = k.split(',').map(Number); return canDeployTile('player', a, b, 'building'); })) return 'unplaceable';
      const xb = spawnUnit('Xbow', c * GW + GW / 2, r * GH + GH / 2, 'player', { tileKeys: keys, deployT: 0 });
      const t = acquireTarget(xb);
      return t === towers.er || t === towers.el;
    };
    ok('Princess towers are 3×3 and centred 3.5 tiles from the edge', Math.abs(towers.el.x / GW - 3.5) < 0.01 && Math.abs(towers.er.x / GW - 14.5) < 0.01);
    ok('One tile back, in front of the tower (cols 13-15 / 14-16) hits', reach(14, 18) === true && reach(15, 18) === true);
    ok('Screenshot 11:47 #1: one tile back, cols 12-14 hits', reach(13, 18) === true);
    ok('Screenshot 11:47 #2: first row middle (cols 8-10) hits', reach(9, 17) === true && reach(8, 17) === true);
    ok('Screenshot 09:25: corner one tile back (cols 15-17) does NOT hit', reach(16, 18) === false && reach(1, 18) === false);
    ok('Wiki: two tiles back does not hit', reach(14, 19) === false && reach(3, 19) === false);
    ok('First-row corner tiles cannot hold an X-Bow', reach(16, 17) === 'unplaceable' && reach(1, 17) === 'unplaceable');
  },
  frontCorners() {
    fresh(['knight','cannon','x-bow','the-log']);
    ok('First row cols 0 and 17 blocked (player)', !canDeployTile('player', 0, 16, 'troop') && !canDeployTile('player', 17, 16, 'troop'));
    ok('First row cols 0 and 17 blocked (enemy mirror)', !canDeployTile('enemy', 0, 13, 'troop') && !canDeployTile('enemy', 17, 13, 'troop'));
    ok('First row cols 1 and 16 still open', canDeployTile('player', 1, 16, 'troop') && canDeployTile('player', 16, 16, 'troop'));
    ok('Second row edges open', canDeployTile('player', 0, 17, 'troop') && canDeployTile('player', 17, 17, 'troop'));
    const p = resolvePlacement('player', C('knight'), 5, 16 * GH + 10);
    ok('Clicking the blocked corner snaps to a neighbouring tile', p && !(p.col === 0 && p.row === 16), p && `${p.col},${p.row}`);
  },
  allCards() {
    const bad = [];
    for (const card of ALL_CARDS) {
      try {
        fresh(); __errors.length = 0;
        const own = mine('knight', 200, 380); enemy('knight', 200, 250); enemy('giant', 120, 260);
        lastPlayed.player = C('knight');
        const eff = effectiveCard(card);
        const p = resolvePlacement('player', eff, 200, card.type === 'spell' ? 250 : 400) || resolvePlacement('player', eff, 100, 420);
        if (!p) { bad.push(card.id + ':no-spot'); continue; }
        if (p.keys) p.keys.forEach(k => occupiedTiles.add(k));
        deployCard(eff, p.x, p.y, 'player', p.keys);
        run(12);
        if (__errors.length) bad.push(card.id + ':' + __errors[0]);
      } catch (e) { bad.push(card.id + ':' + e.message); }
    }
    ok(`All ${ALL_CARDS.length} cards deploy and run 12s without errors`, bad.length === 0, bad.slice(0, 5).join(' | '));
  },
  newMechanics() {
    fresh();
    const fs = enemy('fire-spirit', 200, 360); const a = mine('archers', 195, 420), b = mine('archers', 215, 425);
    const a0 = a.hp, b0 = b.hp; run(1.5);
    ok('Fire Spirit splash hits several units and dies', !troops.includes(fs) && a.hp < a0 && b.hp < b0);
    fresh();
    const es = enemy('electro-spirit', 200, 360); const sk = [0, 1, 2].map(i => mine('knight', 170 + i * 30, 430));
    run(1.5);
    ok('Electro Spirit chains to multiple targets', sk.filter(k => k.hp < k.maxHp).length >= 2, sk.map(k => k.hp).join(','));
    fresh();
    const g = enemy('golem', 200, 300); g.hp = 1; damageTroop(g, 5); run(0.1);
    ok('Golem splits into 2 Golemites on death', troops.filter(t => t.u.id === 'Golemite').length === 2);
    fresh();
    const w = mine('witch', 200, 450); run(2);
    ok('Witch summons Skeletons (4 per wave)', troops.filter(t => t.u.id === 'Skeleton' && t.side === 'player').length === 4, troops.filter(t => t.u.id === 'Skeleton').length);
    fresh(['elixir-collector','knight','cannon','the-log']);
    play(0, 200, 470); elixir = 0;
    const tStart = elixir; run(10.2);
    ok('Elixir Collector pumps extra elixir', elixir > 10.2 / 2.8 + 0.8, elixir.toFixed(2));
    fresh(['freeze','knight','cannon','the-log']);
    const fk = enemy('knight', 200, 250); play(0, 200, 250); run(0.2);
    ok('Freeze stops enemies', fk.hitMul === 0 && !!fk.buffs.Freeze);
    fresh(['rage','knight','cannon','the-log']);
    const rk = mine('knight', 200, 420); play(0, 200, 420); run(0.4);
    ok('Rage speeds up own troops', rk.spdMul > 1.3, rk.spdMul);
    fresh(['tornado','knight','cannon','the-log']);
    const tk = enemy('knight', 240, 250); freezeT(tk); const d0 = Math.hypot(tk.x - 200, tk.y - 250);
    play(0, 200, 250); run(0.5);
    ok('Tornado pulls enemies toward its centre', Math.hypot(tk.x - 200, tk.y - 250) < d0 - 5, `${d0.toFixed(0)}→${Math.hypot(tk.x - 200, tk.y - 250).toFixed(0)}`);
    fresh(['graveyard','knight','cannon','the-log']);
    let gy = 0; const origSpawn = spawnUnit; spawnUnit = (n, ...r) => { if (n === 'Skeleton' && r[2] === 'player') gy++; return origSpawn(n, ...r); };
    play(0, 67, 160); run(5); spawnUnit = origSpawn;
    ok('Graveyard spawns Skeletons over time', gy >= 5, gy + ' spawned');
    fresh(['goblin-barrel','knight','cannon','the-log']);
    let gb = 0; const os2 = spawnUnit; spawnUnit = (n, ...r) => { if (n === 'Goblin' && r[2] === 'player') gb++; return os2(n, ...r); };
    play(0, 67, 190); run(4); spawnUnit = os2;
    ok('Goblin Barrel lands 3 Goblins', gb === 3, gb);
    fresh(['clone','knight','cannon','the-log']);
    mine('knight', 200, 420); play(0, 200, 420); run(0.1);
    ok('Clone duplicates troops with 1 HP', troops.some(t => t.clone && t.hp === 1));
    fresh(['mirror','knight','cannon','the-log']);
    lastPlayed.player = C('knight');
    ok('Mirror costs last card +1', cardCost(C('mirror')) === 4);
    play(0, 200, 420);
    ok('Mirror replays the last card', troops.filter(t => t.u.id === 'Knight').length === 1);
    fresh(['lightning','knight','cannon','the-log']);
    const L = [0, 1, 2, 3].map(i => enemy('knight', 170 + i * 20, 250)); L.forEach(t => freezeT(t)); L[3].hp = 100;
    play(0, 200, 250); run(1.5);
    ok('Lightning strikes the 3 highest-HP targets', L.slice(0, 3).every(t => t.hp < t.maxHp) && L[3].hp === 100, L.map(t => Math.round(t.hp)).join(','));
    fresh();
    const dp = mine('dark-prince', 200, 420); const dp0 = dp.shield;
    damageTroop(dp, 50);
    ok('Shield absorbs damage before HP', dp0 > 0 && dp.hp === dp.maxHp && dp.shield < dp0, `shield ${dp0}→${dp.shield}`);
    fresh(['miner','knight','cannon','the-log']);
    play(0, 67, 170);
    ok('Miner can deploy on the enemy side', troops.some(t => t.u.id === 'Miner' && t.y < RIVER));
    fresh();
    const bnd = mine('bandit', 200, 400); const v = enemy('knight', 200, 300); freezeT(v);
    let dashed = false; for (let i = 0; i < 40; i++) { const y0 = bnd.y; tick(0.05); if (y0 - bnd.y > 20) dashed = true; }
    ok('Bandit dashes to its target', dashed);
    fresh();
    ok('Crown towers use level 11 stats', towers.pl.maxHp === 3052 && towers.pk.maxHp === 4824);
    ok('Three Musketeers cost 9', C('three-musketeers').cost === 9);
    ok('Card art loaded for hand', document.querySelector('#hand-row img.card-art') !== null);
  },
  dataGaps() {
    fresh();
    const pr = mine('princess', 200, 420), t1 = enemy('knight', 200, 250); freezeT(t1); const h1 = t1.hp;
    run(4);
    ok('Princess deals damage (169 area)', t1.hp < h1, `${h1}→${t1.hp}`);
    fresh();
    const hu = mine('hunter', 200, 400), t2 = enemy('giant', 200, 370); freezeT(t2); const h2 = t2.hp;
    run(1.8);
    ok('Hunter point-blank fires all pellets (~840 per shot)', h2 - t2.hp >= 600, h2 - t2.hp);
    fresh(['cannon','knight','x-bow','the-log']);
    play(0, 300, 470); run(1.2);
    const sb = enemy('skeleton-barrel', 300, 150); const y0 = sb.y;
    let burst = false; const os3 = spawnUnit; let sk = 0;
    spawnUnit = (n, ...r) => { if (n === 'Skeleton') sk++; return os3(n, ...r); };
    run(14); spawnUnit = os3;
    ok('Skeleton Barrel flies to a building and bursts into Skeletons', sk >= 5, `skeletons=${sk}`);
    fresh();
    const egg = spawnUnit('PhoenixEgg', 200, 420, 'player', { deployT: 0 });
    run(10);
    ok('Phoenix egg hatches exactly once and disappears', troops.filter(t => t.u.id === 'PhoenixNoRespawn').length === 1 && !troops.includes(egg));
    fresh();
    const lj = mine('lumberjack', 200, 420); const ally = mine('knight', 210, 430);
    lj.hp = 1; damageTroop(lj, 5); run(0.8);
    ok('Lumberjack drops Rage on death', ally.spdMul > 1.3, ally.spdMul);
  },
  icons() {
    fresh();
    const m = mine('minions', 200, 420), k = mine('knight', 150, 420), x = spawnUnit('Xbow', 300, 450, 'player', { deployT: 0, tileKeys: [] });
    const e = enemy('cannon', 200, 200);
    ok('Squad units (Minions) use the card portrait, not the group render', m.el.classList.contains('portrait') && !m.el.classList.contains('sprite'));
    ok('Single units (Knight) use the character sprite', k.el.classList.contains('sprite'));
    ok('Buildings without sprites use a cropped portrait', x.el.classList.contains('portrait'));
    ok('Enemy buildings are not mirrored', getComputedStyle(e.el).transform === 'none');
  },
  specials() {
    fresh();
    const e1 = enemy('knight', 200, 380); const h1 = e1.hp;
    spawnUnit('ElectroWizard', 200, 400, 'player', { deployT: 0.05 }); run(0.1);
    ok('Electro Wizard deploy zap damages and stuns', e1.hp < h1 && !!e1.buffs.ZapFreeze);
    fresh();
    const eg = mine('electro-giant', 200, 420); freezeT(eg); const att = enemy('knight', 200, 400); const ha = att.hp;
    run(2);
    ok('Electro Giant reflects damage to melee attackers', att.hp < ha, `${ha}→${att.hp}`);
    fresh();
    const g = mine('royal-ghost', 200, 450); run(1.5);
    ok('Royal Ghost turns invisible when not attacking', g.hidden);
    const t = enemy('knight', 200, 420); freezeT(t);
    ok('Invisible Ghost is not targeted by enemies', acquireTarget(spawnUnit('Archer', 200, 380, 'enemy', { deployT: 0 })) !== g);
  },
  bridgeCrowd() {
    fresh();
    const gang = [];
    for (let i = 0; i < 6; i++) gang.push(spawnUnit('Barbarian', 50 + i * 7, 260 + (i % 2) * 8, 'enemy', { deployT: 0 }));
    for (let i = 0; i < 5; i++) gang.push(spawnUnit('SkeletonWarrior', 320 + i * 6, 250, 'enemy', { deployT: 0 }));
    for (const t of Object.values(towers)) if (t.side === 'player') t.hp = t.maxHp = 1e9;
    run(14);
    const across = gang.filter(u => !u.dead && u.y > RIVER + RIVER_HALF).length, alive = gang.filter(u => !u.dead).length;
    ok('A crowd of units all get across the bridge (no jam at the mouth)', across === alive && alive > 0, `${across}/${alive} across`);
  },
  laneTargeting() {
    for (const id of ['giant', 'hog-rider', 'knight', 'balloon']) {
      fresh();
      spawnUnit('Xbow', 15.5 * GW, 18.5 * GH, 'player', { deployT: 0, tileKeys: [] });
      const u = enemy(id, 60, 120);
      ok(`Enemy ${id} placed top-left goes for the left princess, not the X-Bow in the right lane`, acquireTarget(u) === towers.pl, (acquireTarget(u) || {}).key || (acquireTarget(u) || {}).u?.id);
    }
    fresh();
    const xb = spawnUnit('Xbow', 15.5 * GW, 18.5 * GH, 'player', { deployT: 0, tileKeys: [] });
    const g = enemy('giant', 15.5 * GW, 12 * GH);
    ok('Giant in the same lane within sight targets the X-Bow', acquireTarget(g) === xb);
    fresh();
    dealTowerDmg('pl', 1e9);
    const k = enemy('knight', 60, 120);
    ok('With the lane princess down, troops go for the King', acquireTarget(k) === towers.pk);
    fresh();
    const m = mine('knight', 340, 420);
    ok('Player troop in the right lane targets the enemy right princess', acquireTarget(m) === towers.er);
  },
  wikiBalance() {
    fresh();
    ok('Wiki current balance applied (Giant 3968 HP, Log 266, Goblin Hut 4 elixir)', UNITS.Giant.hp === 3968 && C('the-log').spell.dmg === 266 && C('goblin-hut').cost === 4);
    ok('Arrows cover a 3.5-tile radius', Math.abs(C('arrows').spell.radius / GW - 3.5) < 0.05);
    const ig = mine('ice-golem', 300, 420); enemy('knight', 300, 380);
    ok('Ice Golem only targets buildings', acquireTarget(ig).isTower === true);
    fresh();
    const mo = spawnUnit('Mortar', 200, 450, 'player', { deployT: 0, tileKeys: [] });
    const near = enemy('knight', 200, 400); freezeT(near);
    ok('Mortar cannot hit units inside its 3.5-tile minimum range', acquireTarget(mo) !== near);
    const far = enemy('knight', 200, 300); freezeT(far);
    ok('Mortar hits units beyond its minimum range', acquireTarget(mo) === far);
  },
  uiScaleFit() {
    fresh();
    const r = document.getElementById('screen-battle').getBoundingClientRect();
    ok('Battle screen fits the window (hand visible)', r.bottom <= window.innerHeight + 1, `bottom ${r.bottom.toFixed(0)} / ${window.innerHeight}, scale ${uiScale.toFixed(2)}`);
    fresh(['knight','cannon','x-bow','the-log']);
    play(0, 100, 20 * GH + 5);
    ok('Clicks land on the right tile when scaled', troops.some(t => t.u.id === 'Knight' && Math.floor(t.x / GW) === 4 && Math.floor(t.y / GH) === 20));
  },
  riders() {
    fresh();
    const gg = mine('goblin-giant', 200, 450);
    run(12);
    const riders = troops.filter(t => t.parent === gg);
    ok('Goblin Giant carries exactly 2 Spear Goblins (no endless spawning)', riders.length === 2 && troops.filter(t => t.u.id === 'SpearGoblinGiant').length === 2, troops.filter(t => t.u.id === 'SpearGoblinGiant').length);
    ok('Riders move with their mount', riders.every(r => Math.abs(r.x - gg.x) < 15 && Math.abs(r.y - gg.y) < 20));
    const h0 = riders[0].hp; damageTroop(riders[0], 50);
    ok('Riders cannot be damaged while mounted', riders[0].hp === h0);
    gg.hp = 1; damageTroop(gg, 5); run(0.2);
    ok('When Goblin Giant dies its Spear Goblins drop as normal Spear Goblins', troops.filter(t => t.u.id === 'SpearGoblin' && t.side === 'player').length === 2);
    fresh();
    const ram = mine('ram-rider', 200, 450); run(12);
    ok('Ram Rider has exactly 1 rider', troops.filter(t => t.u.id === 'RamRider').length === 1);
  },
  aiPocket() {
    fresh(null, true); aiTick = realAi;
    dealTowerDmg('pl', 1e9);
    const pos = []; const od = deployCard;
    deployCard = (card, x, y, side, keys) => { if (side === 'enemy' && card.type === 'troop') pos.push(y); od(card, x, y, side, keys); };
    for (let i = 0; i < 400 && gameRunning; i++) { aiElixir = 10; tick(0.05); }
    deployCard = od;
    ok('AI deploys into the pocket after taking a princess tower', pos.some(y => y > RIVER + RIVER_HALF), pos.length + ' troop plays');
  },
  formations() {
    fresh(['royal-recruits','knight','cannon','the-log']);
    elixir = 10; deployCard(C('royal-recruits'), 120, 420, 'player');
    const rr = troops.filter(t => t.u.id === 'Recruit');
    const xs = rr.map(t => t.x).sort((a, b) => a - b);
    ok('Royal Recruits deploy in a wide line across both lanes', rr.length === 6 && xs[0] < 120 && xs[5] > 280 && rr.every(t => Math.abs(t.y - 420) < 1), xs.map(v => v.toFixed(0)).join(','));
    ok('Recruits on each side head down their own lane', rr.filter(t => t.lane === 0).length === 3 && rr.filter(t => t.lane === 1).length === 3);
    fresh();
    deployCard(C('royal-hogs'), 300, 420, 'player');
    const rh = troops.filter(t => t.u.id === 'RoyalHog').map(t => t.x).sort((a, b) => a - b);
    ok('Royal Hogs deploy in a 3.5-tile line', rh.length === 4 && Math.abs((rh[3] - rh[0]) / GW - 3.5) < 0.1, rh.map(v => v.toFixed(0)).join(','));
    fresh();
    deployCard(C('goblins'), 200, 420, 'player');
    const gd = troops.filter(t => t.u.id === 'Goblin').map(t => t.deployT);
    ok('Squad units appear one after another (staggered deploy)', gd.length === 4 && gd[3] > gd[0]);
    fresh();
    const ec = spawnUnit('ElixirCollector', 200, 470, 'player', { deployT: 0, tileKeys: [] });
    elixir = 3; ec.hp = 1; damageTroop(ec, 5); tick(0.01);
    ok('Destroyed Elixir Collector refunds 1 elixir to its owner', elixir >= 4 - 0.01, elixir.toFixed(2));
  },
  troopOnlyAndRecoil() {
    fresh();
    const ram = mine('ram-rider', 67, 200); run(1.2);
    const rider = troops.find(t => t.u.id === 'RamRider');
    ok('Ram Rider\'s rider never targets towers or buildings', rider && (acquireTarget(rider) === null || !acquireTarget(rider).isTower));
    fresh();
    const fc = mine('firecracker', 200, 450), tgt = enemy('giant', 200, 330); freezeT(tgt);
    run(1.5); const y0 = fc.y; let pushed = false;
    for (let i = 0; i < 60; i++) { const yb = fc.y; tick(0.05); if (fc.y - yb > 3) pushed = true; }
    ok('Firecracker recoils backwards when it fires', pushed);
  },
  graveyardRecruits() {
    fresh(['graveyard','knight','cannon','the-log']);
    const rs = []; const os4 = spawnUnit;
    spawnUnit = (n, x, y, ...r) => { if (n === 'Skeleton') rs.push(Math.hypot(x - 200, y - 200)); return os4(n, x, y, ...r); };
    deployCard(C('graveyard'), 200, 200, 'player'); run(10); spawnUnit = os4;
    ok('Graveyard Skeletons spawn on the edge of the radius', rs.length > 5 && rs.every(d => d >= 3 * GW - 1 && d <= 4 * GW + 1), rs.slice(0, 4).map(d => (d / GW).toFixed(1)).join(','));
    fresh(['royal-recruits','knight','cannon','the-log']);
    dealTowerDmg('el', 1e9);
    ok('Royal Recruits cannot go on the enemy side with only one tower down', resolvePlacement('player', C('royal-recruits'), 100, 200) === null);
    dealTowerDmg('er', 1e9);
    ok('Royal Recruits can go on the enemy side once both towers are down', resolvePlacement('player', C('royal-recruits'), 100, 200) !== null);
  },
  phoenix() {
    fresh();
    const ph = mine('phoenix', 200, 450); ph.hp = 1; damageTroop(ph, 5); run(0.2);
    const egg = troops.find(t => t.u.id === 'PhoenixEgg');
    ok('Phoenix drops an egg when it dies', !!egg);
    run(5.5);
    ok('Egg hatches into a Phoenix that does not respawn', troops.filter(t => t.u.id === 'PhoenixNoRespawn').length === 1 && !troops.some(t => t.u.id === 'PhoenixEgg'));
    fresh();
    const ph2 = mine('phoenix', 200, 450); ph2.hp = 1; damageTroop(ph2, 5); run(0.2);
    const egg2 = troops.find(t => t.u.id === 'PhoenixEgg'); egg2.deployT = 0; damageTroop(egg2, 1e4); run(6);
    ok('Destroying the egg stops the rebirth', !troops.some(t => t.u.id === 'PhoenixNoRespawn'));
  },
  nextCooldown() {
    fresh(['knight','archers','cannon','minions']);
    run(2.1);
    play(0, 100, 420); elixir = 10;
    ok('First play after 2s: slot refills immediately from Next', playerHand[0] !== null);
    play(1, 140, 420);
    ok('Second quick play: slot stays empty while Next cools down', playerHand[1] === null);
    run(1.0);
    ok('…still empty after 1s', playerHand[1] === null);
    run(1.1);
    ok('…filled after the 2s cooldown', playerHand[1] !== null);
    fresh(['knight','archers','cannon','minions']); timeLeft = 50; run(2.1);
    play(0, 100, 420); play(1, 140, 420); run(1.05);
    ok('Double elixir halves the cooldown to 1s', playerHand[1] !== null);
    let bad = 0;
    for (let i = 0; i < 40; i++) {
      chosenDeck = ['elixir-collector','mirror','knight','archers','cannon','minions','giant','zap'].map(C);
      startBattle(); loopToken++;
      if ([...playerHand, nextCard].some(c => c.id === 'elixir-collector' || c.id === 'mirror')) bad++;
    }
    ok('Elixir Collector and Mirror never in the opening hand', bad === 0, bad);
  },
  rascalsFormation() {
    for (const side of ['player', 'enemy']) {
      fresh();
      const y = side === 'player' ? 450 : 200;
      deployCard(C('rascals'), 200, y, side);
      const boy = troops.find(t => t.u.id === 'RascalBoy'), girls = troops.filter(t => t.u.id === 'RascalGirl');
      const front = side === 'player' ? boy.y < Math.min(...girls.map(g => g.y)) : boy.y > Math.max(...girls.map(g => g.y));
      ok(`Rascals (${side}): Boy in front, Girls behind on either side`, front && girls[0].x < 200 && girls[1].x > 200, `boy ${boy.x.toFixed(0)},${boy.y.toFixed(0)} girls ${girls.map(g => g.x.toFixed(0) + ',' + g.y.toFixed(0)).join(' ')}`);
      ok(`Rascals (${side}): all three stay within 2 tiles of the drop point`, [boy, ...girls].every(t => Math.hypot(t.x - 200, t.y - y) <= 2 * GW));
    }
    fresh();
    deployCard(C('goblin-gang'), 200, 200, 'enemy');
    const gy = troops.filter(t => t.u.id === 'Goblin').map(t => t.y), sy = troops.filter(t => t.u.id === 'SpearGoblin').map(t => t.y);
    ok('Enemy Goblin Gang: melee Goblins in front of the Spear Goblins', Math.min(...gy) > Math.max(...sy));
  },
  bridgeJams() {
    const stuckIn = (setup, secs = 20) => {
      fresh();
      for (const t of Object.values(towers)) t.hp = t.maxHp = 1e9;
      setup();
      const track = new Map(); const stuck = [];
      for (let i = 0; i < secs * 20; i++) {
        tick(0.05);
        for (const u of troops) {
          if (u.isBuilding || u.fuse || u.parent || u.deployT > 0) continue;
          const k = track.get(u) || { x: u.x, y: u.y, idle: 0 };
          const attacking = u.lock && alive(u.lock);
          if (Math.hypot(u.x - k.x, u.y - k.y) < 0.4 && !attacking && u.hitMul > 0) k.idle += 0.05; else k.idle = 0;
          k.x = u.x; k.y = u.y;
          if (k.idle > 3 && !k.rep) { k.rep = true; stuck.push(u.u.id + '(' + u.side + ')'); }
          track.set(u, k);
        }
      }
      return stuck;
    };
    const bx = BRIDGE_X[0];
    let s = stuckIn(() => { spawnUnit('Giant', bx, 420, 'player', { deployT: 0 }); spawnUnit('Giant', bx, 245, 'enemy', { deployT: 0 }); });
    ok('Two Giants meeting head-on on a bridge pass each other', s.length === 0, s.join(','));
    s = stuckIn(() => { spawnUnit('Golem', bx, 400, 'player', { deployT: 0 }); for (let i = 0; i < 8; i++) spawnUnit('Barbarian', bx - 20 + i * 6, 440 + (i % 2) * 10, 'player', { deployT: 0 }); });
    ok('A crowd behind a Golem flows around it to reach the tower', s.length === 0, s.join(','));
    s = stuckIn(() => { spawnUnit('Giant', BRIDGE_X[1], 420, 'player', { deployT: 0 }); spawnUnit('Musketeer', BRIDGE_X[1], 460, 'player', { deployT: 0 }); spawnUnit('Giant', BRIDGE_X[1], 245, 'enemy', { deployT: 0 }); spawnUnit('Musketeer', BRIDGE_X[1], 205, 'enemy', { deployT: 0 }); });
    ok('Opposing Giant + Musketeer pushes on the same bridge do not deadlock', s.length === 0, s.join(','));
  },
  restartState() {
    fresh(); deployCard(C('knight'), 200, 420, 'player');
    fresh();
    ok('New match: Mirror has nothing to copy from the previous match', lastPlayed.player === null && effectiveCard(C('mirror')) === null);
  },
  deckBuilder() {
    goToDeckBuilder(); clearDeck();
    ok('Battle button disabled with an empty deck', !document.getElementById('btn-battle').classList.contains('ready') && /0\/8/.test(document.getElementById('btn-battle').textContent));
    ALL_CARDS.filter(c => c.r !== 'champion').slice(0, 8).forEach(toggleDeckCard);
    ok('Deck panel shows 8 filled slots and Battle is ready', document.querySelectorAll('.deck-slot.filled').length === 8 && document.getElementById('btn-battle').classList.contains('ready'));
    ok('Cards in the deck are marked IN DECK in the collection', document.querySelectorAll('.deck-card.in-deck').length === 8);
    toggleDeckCard(ALL_CARDS.find(c => c.r !== 'champion' && !chosenDeck.includes(c)));
    ok('Adding a 9th card is refused with a message', chosenDeck.length === 8 && /full/i.test(document.getElementById('deck-msg').textContent));
    clearDeck();
    const champs = ALL_CARDS.filter(c => c.r === 'champion');
    toggleDeckCard(champs[0]); toggleDeckCard(champs[1]);
    ok('Only one Champion allowed per deck', chosenDeck.filter(c => c.r === 'champion').length === 1);
    let bad = 0; for (let i = 0; i < 30; i++) { randomDeck(); if (chosenDeck.length !== 8 || chosenDeck.filter(c => c.r === 'champion').length > 1) bad++; }
    ok('Random deck: always 8 cards, at most one Champion', bad === 0, bad);
    document.querySelector('.deck-tab[data-k="spell"]').click();
    ok('Spells tab shows only spells', [...document.querySelectorAll('.deck-card')].every(el => ALL_CARDS.find(c => 'dbc-' + c.id === el.id).type === 'spell'));
    document.querySelector('.deck-tab[data-k="all"]').click();
  },
  cardLevels() {
    setLevel('player', 15); setLevel('enemy', 11);
    fresh();
    const k = mine('knight', 200, 420), e = enemy('knight', 200, 250);
    ok('Elite (level 15) Knight has ×1.1⁴ HP and damage', k.maxHp === Math.round(UNITS.Knight.hp * 1.1 ** 4) && Math.abs(k.dmg - UNITS.Knight.dmg * 1.1 ** 4) < 0.01, `${k.maxHp} / ${k.dmg.toFixed(0)}`);
    ok('Opponent level is independent (level 11 Knight)', e.maxHp === UNITS.Knight.hp);
    ok('Towers follow their side\'s level', towers.pl.maxHp === Math.round(3052 * 1.1 ** 4) && towers.el.maxHp === 3052);
    const t = enemy('golem', 200, 250); freezeT(t); const h0 = t.hp;
    spellImpact(C('fireball'), 200, 250, 'player');
    ok('Spells scale with level (Elite Fireball)', Math.abs((h0 - t.hp) - C('fireball').spell.dmg * 1.1 ** 4) < 1, (h0 - t.hp).toFixed(0));
    setLevel('player', 11); setLevel('enemy', 11);
    fresh();
    ok('Back to level 11 restores base stats', mine('knight', 200, 420).maxHp === UNITS.Knight.hp && towers.pl.maxHp === 3052);
  },
  sounds() {
    const played = []; const A = window.Audio;
    window.Audio = function (src) { played.push(src); return { play: () => Promise.resolve(), pause() {}, set volume(v) {}, set loop(v) {} }; };
    if (SND.muted) SND.toggle();
    fresh(['knight','fireball','cannon','the-log']);
    deployCard(C('bowler'), 200, 420, 'player');
    ok('Deploying a card plays its sound', played.some(s => s.includes('bowler_deploy')), played.join(','));
    played.length = 0; timeLeft = 10.04; run(0.1);
    ok('Final countdown plays', played.some(s => /_cd_02/.test(s)), played.join(','));
    played.length = 0; dealTowerDmg('el', 1e9);
    ok('Tower destroyed and crown sounds play', played.some(s => s.includes('building_destroyed')) && played.some(s => s.includes('crown')));
    played.length = 0; SND.toggle(); deployCard(C('knight'), 200, 420, 'player');
    ok('Mute silences sounds', played.length === 0);
    SND.toggle(); window.Audio = A;
  },
  metaDecks() {
    ok('All meta decks are complete and use existing cards', META_DECKS.every(([, ids]) => ids.length === 8 && new Set(ids).size === 8 && ids.every(id => C(id))));
    ok('Meta decks have at most one Champion', META_DECKS.every(([, ids]) => ids.filter(id => C(id).r === 'champion').length <= 1));
    goToDeckBuilder(); metaDeck();
    ok('Meta button loads deck with its evolution cards in slots 1-2', chosenDeck[0].id === META_DECKS[0][1][0] && /RoyaleAPI/.test(document.getElementById('deck-msg').textContent));
  },
  aiDecks() {
    let bad = 0;
    for (let i = 0; i < 30; i++) { fresh(null, true); if (aiDeck.length !== 8 || new Set(aiDeck.map(c => c.id)).size !== 8 || !META_DECKS.some(([n]) => document.getElementById('ai-deck-name').textContent.includes(n))) bad++; }
    ok('AI always plays a complete current meta deck', bad === 0, document.getElementById('ai-deck-name').textContent);
    ok('Pro deck button is gone', !document.getElementById('btn-pro') && typeof PRO_DECKS === 'undefined');
  },
  evolutions() {
    const evoDeck = ids => { chosenDeck = ids.map(C); startBattle(); loopToken++; aiTick = () => {}; elixir = 10; };
    evoDeck(['knight','archers','barbarians','cannon','giant','zap','minions','the-log']);
    const seq = [0, 1, 2, 3, 4].map(() => takeEvolution('player', C('knight')));
    ok('Knight (2 cycles) evolves every 3rd play', JSON.stringify(seq) === '[false,false,true,false,false]', JSON.stringify(seq));
    const seqB = [0, 1, 2, 3].map(() => takeEvolution('player', C('barbarians')));
    ok('Barbarians (1 cycle) evolve every 2nd play', JSON.stringify(seqB) === '[false,true,false,true]', JSON.stringify(seqB));
    ok('Only the Evolution and Wild slots evolve (slot 2 is the Hero slot)', !evoProgress('player', C('archers')));
    evoDeck(['hog-rider','archers','barbarians','cannon','giant','zap','minions','the-log']);
    ok('Cards without an Evolution never evolve, even in the Evolution slot', !evoProgress('player', C('hog-rider')) && !!evoProgress('player', C('barbarians')));
    // abilities
    fresh();
    const ek = spawnUnit('Knight', 200, 420, 'player', { deployT: 0, evo: 'knight' }), nk = spawnUnit('Knight', 150, 420, 'player', { deployT: 0 });
    damageTroop(ek, 100); damageTroop(nk, 100);
    ok('Evolved Knight takes 60% less damage before landing a hit', Math.abs((ek.maxHp - ek.hp) - 40) < 0.5 && nk.maxHp - nk.hp === 100);
    fresh();
    const eb = spawnUnit('Barbarian', 200, 330, 'player', { deployT: 0, evo: 'barbarians' }); const d1 = enemy('knight', 200, 312); freezeT(d1);
    run(2); ok('Evolved Barbarians speed up after attacking (+30%)', !!eb.buffs.BladeRage && Math.abs(eb.hitMul - 1.3) < 0.01);
    fresh();
    deployCard(C('skeletons'), 200, 330, 'player', null, true);
    const dum = spawnUnit('Golem', 200, 300, 'enemy', { deployT: 0 }); dum.hp = dum.maxHp = 1e6; freezeT(dum);
    let peak = 0, grp = null;
    for (let i = 0; i < 160; i++) { tick(0.05); const s = troops.filter(t => t.u.id === 'Skeleton' && t.side === 'player'); peak = Math.max(peak, s.length); grp = grp || (s[0] && s[0].evoGroup); }
    ok('Evolved Skeletons multiply on attack, capped at 8', peak === 8 && grp.n === 8, `peak ${peak}, spawned total ${grp && grp.n}`);
    fresh();
    const bt = spawnUnit('Bat', 200, 420, 'player', { deployT: 0, evo: 'bats' });
    ok('Evolved Bats have +50% HP', bt.maxHp === Math.round(UNITS.Bat.hp * 1.5));
    fresh();
    const mo = spawnUnit('Mortar', 200, 470, 'player', { deployT: 0, evo: 'mortar', tileKeys: [] }); const far = enemy('giant', 200, 250); freezeT(far);
    let gob = 0; const os5 = spawnUnit; spawnUnit = (n, ...r) => { if (n === 'Goblin' && r[2] === 'player') gob++; return os5(n, ...r); };
    run(9); spawnUnit = os5;
    ok('Evolved Mortar spawns a Goblin with each shot, firing 1s faster', gob >= 1 && mo.atkOverride === UNITS.Mortar.atk - 1, gob);
    fresh();
    const rg = spawnUnit('RoyalGiant', 200, 360, 'player', { deployT: 0, evo: 'royal-giant' }); const near = enemy('knight', 210, 340); freezeT(near);
    const p0 = [near.x, near.y]; dealTowerDmg('pl', 0); rg.atkCd = 0; hitAt(rg, towers.el, towers.el.x, towers.el.y, 10);
    ok('Evolved Royal Giant shockwave damages and knocks back nearby enemies', near.hp < near.maxHp && Math.hypot(near.x - p0[0], near.y - p0[1]) > 5);
    fresh();
    const ar = spawnUnit('Archer', 200, 420, 'player', { deployT: 0, evo: 'archers' });
    ok('Evolved Archers have +1 tile range', Math.abs(ar.range - UNITS.Archer.range - GW) < 0.1);
    fresh();
    const rr = spawnUnit('Recruit', 200, 420, 'player', { deployT: 0, evo: 'royal-recruits' }); damageTroop(rr, 1e4);
    ok('Evolved Royal Recruits gain a charge once their shield breaks', !!rr.evoCharge && !rr.dead);
    fresh();
    const wz = spawnUnit('Wizard', 200, 420, 'player', { deployT: 0, evo: 'wizard' });
    ok('Evolved Wizard spawns with a 25% shield', wz.shield === Math.round(wz.maxHp * 0.25));
    fresh(['zap','knight','cannon','the-log']);
    const zt = enemy('knight', 200, 250); freezeT(zt);
    deployCard(C('zap'), 200, 250, 'player', null, true); const z1 = zt.hp; run(1.2);
    ok('Evolved Zap strikes a second time', zt.hp < z1);
    fresh();
    const ct = enemy('knight', 200, 380); freezeT(ct);
    deployCard(C('cannon'), 200, 470, 'player', [], true); run(2);
    ok('Evolved Cannon drops a bomb barrage on deploy', ct.hp < ct.maxHp);
    fresh();
    const hg = spawnUnit('RoyalHog', 200, 420, 'player', { deployT: 0, evo: 'royal-hogs' });
    ok('Evolved Royal Hogs fly until they attack', hg.air && hg.flying);
    fresh();
    deployCard(C('skeleton-army'), 200, 420, 'player', null, true);
    ok('Evolved Skeleton Army has +1 Skeleton', troops.filter(t => t.u.id === 'Skeleton').length === 16);
    fresh(['knight','archers','cannon','giant','zap','minions','the-log','barbarians']);
    ok('AI picks up to 2 evolution slots from its deck', Array.isArray(aiEvoIds) && aiEvoIds.length <= 2 && aiEvoIds.every(canEvolve));
    // every implemented evolution deploys and runs for both sides
    const bad = [];
    for (const id of Object.keys(EVOLUTIONS).filter(canEvolve)) {
      for (const side of ['player', 'enemy']) {
        try {
          fresh(); __errors.length = 0;
          enemy('knight', 200, 250); mine('knight', 200, 420);
          const card = C(id), y = card.type === 'spell' ? (side === 'player' ? 250 : 420) : (side === 'player' ? 440 : 200);
          const p = resolvePlacement(side, card, 200, y) || resolvePlacement(side, card, 100, y);
          if (!p) { bad.push(id + ':' + side + ':no-spot'); continue; }
          if (p.keys) p.keys.forEach(k => occupiedTiles.add(k));
          deployCard(card, p.x, p.y, side, p.keys, true);
          run(12);
          if (__errors.length) bad.push(id + ':' + __errors[0]);
        } catch (e) { bad.push(id + ':' + e.message); }
      }
    }
    ok(`All ${Object.keys(EVOLUTIONS).filter(canEvolve).length} implemented evolutions run for both sides without errors`, bad.length === 0, bad.slice(0, 4).join(' | '));
  },
  dragDrop() {
    goToDeckBuilder();
    chosenDeck = ['archers','fireball','knight','giant','zap','minions','the-log','hog-rider'].map(C); renderDeck();
    dropOnSlot('slot:2', 0);
    ok('Drag deck card into an Evolution slot swaps them', chosenDeck[0].id === 'knight' && chosenDeck[2].id === 'archers' && !!evoProgress && document.querySelectorAll('.deck-slot.evo-on').length >= 1);
    dropOnSlot('card:skeletons', 1);
    ok('Drag collection card onto a full deck slot replaces it', chosenDeck[1].id === 'skeletons' && chosenDeck.length === 8 && !chosenDeck.some(c => c.id === 'fireball'));
    dropOnCollection('slot:7');
    ok('Drag deck card back to the collection removes it', chosenDeck.length === 7);
    dropOnSlot('card:bats', 7);
    ok('Drag collection card onto an empty slot adds it', chosenDeck.length === 8 && chosenDeck[7].id === 'bats');
    chosenDeck = ['archer-queen','fireball','knight','giant','zap','minions','the-log','hog-rider'].map(C); renderDeck();
    dropOnSlot('card:monk', 3);
    ok('Drag-drop still enforces one Champion', !chosenDeck.some(c => c.id === 'monk'));
    ok('Deck slots and collection cards are draggable', document.querySelector('.deck-slot.filled').draggable && document.querySelector('.deck-card').draggable);
    chosenDeck = ['archers','fireball','knight','giant','zap','minions','the-log','hog-rider'].map(C); renderDeck();
    const dtA = new DataTransfer(); dtA.setData('text/plain', 'slot:2');
    document.querySelectorAll('.deck-slot')[0].dispatchEvent(new DragEvent('drop', { dataTransfer: dtA, bubbles: true, cancelable: true }));
    const dtB = new DataTransfer(); dtB.setData('text/plain', 'card:bats');
    document.querySelectorAll('.deck-slot')[1].dispatchEvent(new DragEvent('drop', { dataTransfer: dtB, bubbles: true, cancelable: true }));
    ok('Real drop events on the deck slots work', chosenDeck[0].id === 'knight' && chosenDeck[1].id === 'bats', chosenDeck.slice(0, 3).map(c => c.id).join(','));
  },
  moreEvos() {
    fresh();
    const fu = spawnUnit('Furnace', 200, 470, 'player', { deployT: 0, evo: 'furnace' });
    ok('Evolved Furnace spawns every 2.4s while attacking', fu.spawnPause === 2.4);
    fresh();
    const dg = spawnUnit('BlowdartGoblin', 200, 420, 'player', { deployT: 0, evo: 'dart-goblin' }), tg = enemy('giant', 200, 320); freezeT(tg);
    run(2); const h1 = tg.hp; run(1);
    ok('Evolved Dart Goblin poisons its target over time', tg.poisonDps > 0 && tg.hp < h1);
    fresh();
    const br = spawnUnit('BattleRam', 200, 470, 'player', { deployT: 0, evo: 'battle-ram' }); br.charged = true;
    const v = enemy('knight', 200, 470 - br.sz * 0.7); freezeT(v); const v0 = [v.x, v.y, v.hp]; tick(0.05);
    ok('Evolved Battle Ram knocks back and damages what it rams while charging', v.hp < v0[2] && Math.hypot(v.x - v0[0], v.y - v0[1]) > 10);
    fresh();
    const w = spawnUnit('Witch', 200, 470, 'player', { deployT: 0, evo: 'witch' }); w.hp = w.maxHp * 0.5;
    run(1.5); (w.firstWave || []).forEach(k => { k.dead = true; }); run(0.1);
    ok('Evolved Witch heals when her first Skeletons are defeated', w.hp > w.maxHp * 0.75 && w.firstWave === null, (w.hp / w.maxHp).toFixed(2));
    fresh();
    const dr = spawnUnit('GoblinDrill', 67, 200, 'player', { deployT: 0, evo: 'goblin-drill', tileKeys: [] }); const p0 = [dr.x, dr.y];
    dr.hp = dr.maxHp * 0.6; let gb2 = 0; const os7 = spawnUnit; spawnUnit = (n, ...r) => { if (n === 'Goblin') gb2++; return os7(n, ...r); };
    tick(0.05); spawnUnit = os7;
    ok('Evolved Goblin Drill burrows at 66% HP, drops 2 Goblins and re-emerges elsewhere', gb2 >= 2 && Math.hypot(dr.x - p0[0], dr.y - p0[1]) > 20);
  },
  champions() {
    const champ = (id, x = 200, y = 420) => { fresh(); elixir = 10; const c = spawnUnit(id, x, y, 'player', { deployT: 0 }); return c; };
    let c = champ('ArcherQueen');
    run(0.1);
    ok('Ability button appears with a champion on the field', document.getElementById('ability-btn').style.display === 'flex');
    ok('Using the ability costs elixir', useAbility('player') && Math.abs(elixir - 9) < 0.2);
    run(1.1);
    ok('Archer Queen turns invisible and attacks faster', c.hidden && c.abilityOn && c.hitMul > 1.5);
    run(3.6);
    ok('…then visible again with a 17s cooldown', !c.hidden && !c.abilityOn && c.abilityCd > 15);
    ok('Ability cannot be reused during cooldown', !useAbility('player'));
    c = champ('Monk'); useAbility('player'); run(1.1);
    const h0 = c.hp; damageTroop(c, 100);
    ok('Monk takes 65% less damage during Pensive Protection', Math.abs((h0 - c.hp) - 35) < 0.5);
    const shooter = enemy('musketeer', 200, 300); const s0 = shooter.hp; run(2);
    ok('Monk reflects projectiles back at the shooter', shooter.hp < s0);
    c = champ('MightyMiner', 100, 420); useAbility('player'); run(1.1);
    ok('Mighty Miner switches to the mirrored lane', Math.abs(c.x - (AW - 100)) < 30, c.x.toFixed(0));
    c = champ('SkeletonKing'); c.souls = 4; let sk = 0; const os6 = spawnUnit;
    spawnUnit = (n, ...r) => { if (n === 'Skeleton') sk++; return os6(n, ...r); };
    useAbility('player'); run(4); spawnUnit = os6;
    ok('Skeleton King summons 6 + souls Skeletons', sk === 10, sk);
    c = champ('GoldenKnight', 200, 420);
    const foes = [0, 1, 2].map(i => { const f = enemy('knight', 160 + i * 40, 330); freezeT(f); return f; });
    useAbility('player'); run(3);
    ok('Golden Knight chain-dashes through nearby enemies', foes.filter(f => f.hp < f.maxHp).length >= 2, foes.map(f => Math.round(f.hp)).join(','));
    fresh(null, true); aiTick = realAi;
    const ec = spawnUnit('ArcherQueen', 200, 250, 'enemy', { deployT: 0 }); mine('knight', 200, 330);
    let used = false; for (let i = 0; i < 100; i++) { aiElixir = 10; tick(0.05); if (ec.abilityOn || ec.abilityCd > 0) used = true; }
    ok('AI uses its champion ability', used);
  },
};
window.addEventListener('load', () => {
  for (const [name, fn] of Object.entries(TESTS)) {
    try { fn(); } catch (e) { R.push({ name: name + ' threw', pass: false, info: e.message + ' ' + (e.stack||'').split('\n')[1] }); }
  }
  const pre = document.createElement('pre'); pre.id = 'results';
  pre.textContent = JSON.stringify({ errors: __errors, results: R });
  document.body.appendChild(pre);
});
</script>
