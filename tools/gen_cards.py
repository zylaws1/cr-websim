#!/usr/bin/env python3
"""Regenerate the UNITS / ALL_CARDS tables in index.html from RoyaleAPI's cr-api-data.

Usage:  python3 tools/gen_cards.py            # downloads data to /tmp/cr-data, writes /tmp/cr-gen/cards.js
Then paste cards.js over the `const UNITS` / `const ALL_CARDS` block in index.html.
Source: https://github.com/RoyaleAPI/cr-api-data (docs/json). Stats are taken at tournament level 11.
"""
import json, os, urllib.request
D = '/tmp/cr-data/'
os.makedirs(D, exist_ok=True); os.makedirs('/tmp/cr-gen', exist_ok=True)
for f in ['cards.json','cards_stats_characters.json','cards_stats_building.json','cards_stats_spell.json',
          'cards_stats_projectile.json','cards_stats_troop.json','cards_stats_character_buff.json']:
    if not os.path.exists(D + f):
        urllib.request.urlretrieve('https://raw.githubusercontent.com/RoyaleAPI/cr-api-data/master/docs/json/' + f, D + f)
L=lambda f: json.load(open(D+f))
cards=L('cards.json')
ch={x['name']:x for x in L('cards_stats_characters.json')}
bd={x['name']:x for x in L('cards_stats_building.json')}
sp={x['name']:x for x in L('cards_stats_spell.json')}
pj={x['name']:x for x in L('cards_stats_projectile.json')}
tr={x['name']:x for x in L('cards_stats_troop.json')}
bf={x['name']:x for x in L('cards_stats_character_buff.json')}
def by_key(d,key):
    for v in d.values():
        if v.get('key')==key: return v
def resolve(c):
    k,sc=c['key'],c['sc_key']
    if c['type']=='Troop':
        t=tr.get(sc) or by_key(tr,k)
        return ('troop',t, ch.get(t['summon_character']) if t else None)
    if c['type']=='Building':
        b=bd.get(sc) or by_key(bd,k)
        return ('building',b,b)
    s=sp.get(sc) or by_key(sp,k)
    if s: return ('spell',s,s)
    t=tr.get(sc) or by_key(tr,k)
    return ('spelltroop',t, (ch.get(t['summon_character']) or bd.get(t['summon_character'])) if t else None)

GW = 400/18
OFF = {'Common':0,'Rare':2,'Epic':5,'Legendary':8,'Champion':10}
LEVEL = 11
def idx(r): return LEVEL-1-OFF.get(r,0)
def lvl(e, base, rarity=None):
    r = rarity or e.get('rarity') or 'Common'
    arr = e.get(base+'_per_level')
    i = idx(r)
    if arr: return arr[min(i, len(arr)-1)]
    v = e.get(base) or 0
    return round(v * 1.1**i) if v else 0
def px(units): return round((units or 0)/1000*GW, 1)
BUILDING_TARGETERS = {'Giant','HogRider','Balloon','Golem','Golemite','RoyalGiant','LavaHound','Ram','BattleRam','RamRider','Wallbreaker','WallBreakers','ElectroGiant','GoblinGiant','ElixirGolem1','ElixirGolem2','ElixirGolem4','RoyalHog','SkeletonBalloon','GoblinBarrelRocket','IceGolemite'}

buff_effect = {}
def buff(name):
    b = bf.get(name) or {}
    return {'spd': b.get('speed_multiplier',0), 'hit': b.get('hit_speed_multiplier',0),
            'dps': b.get('damage_per_second',0), 'heal': b.get('heal_per_second',0),
            'bld': b.get('building_damage_percent',0), 'ctd': b.get('crown_tower_damage_percent',0),
            'pull': b.get('attract_percentage',0)}

UNITS = {}
def proj_def(name, rarity):
    p = pj.get(name)
    if not p: return None
    d = {'spd': round(p.get('speed',600)/60*GW), 'splash': px(p.get('radius')), 'dmg': lvl(p,'damage',rarity)}
    if p.get('chained_hit_count'): d['chain'] = [p['chained_hit_count'], px(p.get('chained_hit_radius'))]
    if p.get('target_buff'): d['buff'] = [p['target_buff'], (p.get('buff_time') or 0)/1000, buff(p['target_buff'])]
    if p.get('pushback'): d['push'] = px(p['pushback'])
    if p.get('crown_tower_damage_percent'): d['ctd'] = 1 + p['crown_tower_damage_percent']/100
    if p.get('spawn_character'):
        d['spawn'] = [unit(p['spawn_character'], rarity), p.get('spawn_character_count') or p.get('spawn_count') or 1]
    if p.get('aoe_to_air') is False and p.get('aoe_to_ground'): d['groundOnly'] = True
    return d

def unit(name, rarity=None):
    if not name: return None
    if name in UNITS: return name
    e = ch.get(name) or bd.get(name)
    if not e: return None
    UNITS[name] = None
    r = e.get('rarity') or rarity or 'Common'
    isB = name in bd and name not in ch
    u = {'id': name, 'hp': lvl(e,'hitpoints',r), 'atk': (e.get('hit_speed') or 1000)/1000,
         'load': (e.get('load_time') or 0)/1000, 'spd': round((e.get('speed') or 0)/60, 3),
         'range': px(e.get('range')), 'sight': px(e.get('sight_range')), 'deploy': (e.get('deploy_time') or 1000)/1000,
         'sz': max(14, round(2*px(e.get('collision_radius') or 500))), 'splash': px(e.get('area_damage_radius'))}
    if isB: u['building'] = True
    if (e.get('flying_height') or 0) > 0: u['air'] = True
    if e.get('attacks_air') and e.get('attacks_ground'): u['targets'] = 'any'
    elif e.get('attacks_air'): u['targets'] = 'air'
    else: u['targets'] = 'ground'
    if name in BUILDING_TARGETERS or e.get('target_only_towers'): u['targets'] = 'building'
    if e.get('projectile'):
        pd = proj_def(e['projectile'], r)
        if pd and not pd['dmg'] and e.get('custom_first_projectile'):      # Princess: damage on the "first" projectile
            pd = proj_def(e['custom_first_projectile'], r) or pd
        sub = (pj.get(e['projectile']) or {}).get('spawn_projectile')
        if pd and not pd['dmg'] and sub:                                     # Firecracker: damage on the explosion
            sd = proj_def(sub, r)
            if sd: pd['dmg'], pd['splash'] = sd['dmg'], sd['splash']
        if pd and e.get('multiple_projectiles') and not pd['splash']:
            pd['pellets'] = e['multiple_projectiles']                          # Hunter: 10 pellets, fewer hit at range
        if pd:
            u['proj'] = pd; u['dmg'] = pd['dmg']
            if pd['splash'] and not u['splash']: u['splash'] = pd['splash']
    if 'dmg' not in u: u['dmg'] = lvl(e,'damage',r)
    if e.get('buff_on_damage'):                                   # Electro Wizard: every hit stuns
        u['hitBuff'] = [e['buff_on_damage'], (e.get('buff_on_damage_time') or 500)/1000, buff(e['buff_on_damage'])]
    if e.get('spawn_area_object') and sp.get(e['spawn_area_object']):   # deploy blast (E-Wiz zap, Ice Wizard chill)
        z = sp[e['spawn_area_object']]
        if z.get('damage') or z.get('buff'):
            u['deployBlast'] = {'radius': px(z.get('radius')), 'dmg': lvl(z,'damage',r) if z.get('damage') else 0,
                                'ctd': 1 + (z.get('crown_tower_damage_percent') or 0)/100,
                                'buff': [z['buff'], (z.get('buff_time') or 0)/1000, buff(z['buff'])] if z.get('buff') and not z.get('only_own_troops') else None}
    if e.get('reflected_attack_damage'):                           # Electro Giant
        u['reflect'] = [round(e['reflected_attack_damage'] * 1.1**idx(r)), px(e.get('reflected_attack_radius')),
                        e.get('reflected_attack_buff'), (e.get('reflected_attack_buff_duration') or 500)/1000, buff(e.get('reflected_attack_buff'))]
    if e.get('buff_when_not_attacking') == 'Invisibility':         # Royal Ghost
        u['invis'] = (e.get('buff_when_not_attacking_time') or 1800)/1000
    if e.get('shield_hitpoints'): u['shield'] = round(e['shield_hitpoints'] * u['hp'] / max(1, e.get('hitpoints') or 1))
    if e.get('minimum_range'): u['minRange'] = px(e['minimum_range'])          # Mortar can't hit point-blank
    if e.get('target_only_troops'): u['troopsOnly'] = True                    # Ram Rider's bola: troops only
    if e.get('ignore_targets_with_buff'): u['skipBuffed'] = e['ignore_targets_with_buff']
    if e.get('attack_push_back') and e.get('projectile') and name == 'Firecracker': u['recoil'] = px(e['attack_push_back'])
    if e.get('kamikaze'): u['kamikaze'] = True
    if e.get('self_as_aoe_center'): u['selfSplash'] = True
    if e.get('jump_enabled'): u['jumps'] = True
    if e.get('hides_when_not_attacking'): u['hides'] = True
    if e.get('life_time'): u['life'] = e['life_time']/1000
    if e.get('mana_collect_amount'): u['elixirGen'] = [e['mana_collect_amount'], e['mana_generate_time_ms']/1000]
    if e.get('mana_on_death_for_opponent'): u['elixirOnDeathOpp'] = e['mana_on_death_for_opponent']
    if e.get('mana_on_death'): u['elixirOnDeath'] = e['mana_on_death']          # Elixir Collector refunds its owner
    if e.get('crown_tower_damage_percent'): u['ctd'] = 1 + e['crown_tower_damage_percent']/100
    if e.get('multiple_targets'): u['multi'] = e['multiple_targets']
    if e.get('death_damage'): u['deathDmg'] = [lvl(e,'death_damage',r), px(e.get('death_damage_radius'))]
    if e.get('death_spawn_projectile'):
        dp = proj_def(e['death_spawn_projectile'], r)
        if dp: u['deathDmg'] = [dp['dmg'], dp['splash'] or 44]
        if dp and dp.get('spawn'): ds_proj = [dp['spawn']]                    # Phoenix: death fireball leaves an egg
        else: ds_proj = []
    else: ds_proj = []
    ds = []
    for a,b in [('death_spawn_character','death_spawn_count'),('death_spawn_character2','death_spawn_count2'),('death_spawn_character3','death_spawn_count2')]:
        if e.get(a): ds.append([unit(e[a], r), e.get(b) or 1])
    ds += ds_proj
    if ds: u['deathSpawn'] = [d for d in ds if d[0]]
    if e.get('spawn_character'):
        u['spawn'] = {'unit': unit(e['spawn_character'], r), 'n': e.get('spawn_number') or 1,
                      'pause': (e.get('spawn_pause_time') or 0)/1000, 'interval': (e.get('spawn_interval') or 0)/1000,
                      'start': (e.get('spawn_start_time') or 0)/1000,
                      'limit': e.get('spawn_limit') or 0, 'destroy': bool(e.get('destroy_at_limit'))}
    if e.get('charge_range'):
        u['charge'] = [px(e['charge_range']), lvl(e,'damage_special',r) or u['dmg']*2, (e.get('charge_speed_multiplier') or 200)/100]
    if e.get('dash_damage') or e.get('dash_max_range'):
        u['dash'] = [px(e.get('dash_min_range')), px(e.get('dash_max_range')), lvl(e,'dash_damage',r) or u['dmg'], (e.get('dash_cooldown') or 800)/1000, px(e.get('dash_radius'))]
    if e.get('variable_damage2'):
        f = u['dmg'] / max(1, (e.get('damage') or (pj.get(e.get('projectile') or '',{}) or {}).get('damage') or u['dmg']))
        u['ramp'] = [[0, u['dmg']], [(e.get('variable_damage_time1') or 2000)/1000, round(e['variable_damage2']*f)],
                     [((e.get('variable_damage_time1') or 2000)+(e.get('variable_damage_time2') or 2000))/1000, round((e.get('variable_damage3') or e['variable_damage2'])*f)]]
    UNITS[name] = {k:v for k,v in u.items() if v not in (None, 0, [], {}) or k in ('dmg','hp')}
    return name

ICONS = {'knight':'⚔️','archers':'🏹','goblins':'👺','giant':'🗿','pekka':'🦾','minions':'😈','balloon':'🎈','witch':'🧙','barbarians':'🪓','golem':'🪨','skeletons':'💀','valkyrie':'🪓','skeleton-army':'☠️','bomber':'💣','musketeer':'🔫','baby-dragon':'🐲','prince':'🤺','wizard':'🧙‍♂️','mini-pekka':'🤖','spear-goblins':'🗡️','giant-skeleton':'💀','hog-rider':'🐗','minion-horde':'👿','ice-wizard':'❄️','royal-giant':'🏰','guards':'🛡️','princess':'👸','dark-prince':'🖤','three-musketeers':'🔫','lava-hound':'🌋','ice-spirit':'🧊','fire-spirit':'🔥','miner':'⛏️','sparky':'⚡','bowler':'🎳','lumberjack':'🪵','battle-ram':'🐏','inferno-dragon':'🐉','ice-golem':'🧊','mega-minion':'👹','dart-goblin':'🎯','goblin-gang':'👺','electro-wizard':'⚡','elite-barbarians':'💪','hunter':'🔫','executioner':'🪓','bandit':'🥷','royal-recruits':'🛡️','night-witch':'🦇','bats':'🦇','royal-ghost':'👻','ram-rider':'🐏','zappies':'🔋','rascals':'👦','cannon-cart':'🛒','mega-knight':'🛡️','skeleton-barrel':'🛢️','flying-machine':'✈️','wall-breakers':'💥','royal-hogs':'🐖','goblin-giant':'👹','fisherman':'🎣','magic-archer':'🏹','electro-dragon':'🐉','firecracker':'🎆','mighty-miner':'⛏️','elixir-golem':'💧','battle-healer':'💖','skeleton-king':'👑','archer-queen':'👸','golden-knight':'🌟','monk':'🧘','skeleton-dragons':'🐲','mother-witch':'🧙‍♀️','electro-spirit':'⚡','electro-giant':'🔌','phoenix':'🐦‍🔥','little-prince':'🤴','goblin-demolisher':'🧨','goblin-machine':'🤖','suspicious-bush':'🌳','goblinstein':'🧟','rune-giant':'🗿','berserker':'😤','boss-bandit':'🦹','spirit-empress':'👸','heal-spirit':'💚','terry':'🧑','cannon':'💥','goblin-hut':'🛖','mortar':'🎯','inferno-tower':'🌋','bomb-tower':'💣','barbarian-hut':'🛖','tesla':'🔌','elixir-collector':'💧','x-bow':'🏹','tombstone':'🪦','furnace':'🔥','goblin-cage':'🪤','goblin-drill':'🕳️','fireball':'🔥','arrows':'🎯','rage':'😡','rocket':'🚀','goblin-barrel':'🛢️','freeze':'🥶','mirror':'🪞','lightning':'🌩️','zap':'⚡','poison':'☠️','graveyard':'⚰️','the-log':'🪵','tornado':'🌪️','clone':'👥','earthquake':'🫨','barbarian-barrel':'🛢️','giant-snowball':'❄️','royal-delivery':'📦','party-rocket':'🎉','void':'🕳️','goblin-curse':'🧿','vines':'🌿','party-hut':'🎉'}

CARDS = []
skipped = []
EVENT = lambda k: k.startswith('super-') or k in ('santa-hog-rider','raging-prince')
for c in cards:
    if EVENT(c['key']): continue
    if c['key'] == 'elixir-collector':
        u = unit('ElixirCollector', 'Rare'); m = UNITS[u]
        CARDS.append({'id':'elixir-collector','name':c['name'],'icon':'💧','r':'rare','cost':c['elixir'],'type':'building','unit':u,'hp':m['hp'],'dmg':0,'targets':'ground'}); continue
    if c['key'] == 'heal-spirit':
        u = unit('HealSpirit', 'Rare'); UNITS[u]['heal'] = [400, 55.6]
        m = UNITS[u]
        CARDS.append({'id':'heal-spirit','name':c['name'],'icon':'💚','r':'rare','cost':c['elixir'],'type':'troop','units':[[u,1]],'spread':0,**{k:m[k] for k in ('hp','dmg','atk','targets','range','splash') if k in m}}); continue
    kind, a, b = resolve(c)
    rar = c['rarity']
    base = {'id': c['key'], 'name': c['name'], 'icon': ICONS.get(c['key'], '❔'), 'r': rar.lower(), 'cost': c['elixir']}
    if c['key'] == 'mirror':
        CARDS.append({**base, 'type':'spell', 'spell':{'kind':'mirror'}, 'splash':0, 'dmg':0, 'targets':'any'}); continue
    if kind == 'troop' and a and b:
        units = [[unit(a['summon_character'], rar), a.get('summon_number') or 1]]
        if a.get('summon_character_second'): units.append([unit(a['summon_character_second'], rar), a.get('summon_character_second_count') or 1])
        units = [u for u in units if u[0]]
        main = UNITS[units[0][0]]
        card = {**base, 'type':'troop', 'units': units, 'spread': px(a.get('summon_radius')) or 0}
        if a.get('summon_width'): card['width'] = px(a['summon_width'])            # Royal Recruits / Royal Hogs deploy in a line
        if a.get('summon_deploy_delay'): card['stagger'] = a['summon_deploy_delay'] / 1000
        if a.get('can_deploy_on_enemy_side'): card['anywhere'] = True
        for k in ('hp','dmg','atk','targets','air','splash','range'): 
            if k in main: card[k] = main[k]
        CARDS.append(card); continue
    if kind == 'building' and a:
        u = unit(a['name'], rar); m = UNITS[u]
        CARDS.append({**base, 'type':'building', 'unit': u, **{k:m[k] for k in ('hp','dmg','atk','targets','range','splash') if k in m}}); continue
    if kind == 'spell' and a:
        s = {'kind':'area', 'radius': px(a.get('radius')), 'dmg': lvl(a,'damage',rar) if a.get('damage') else 0,
             'duration': (a.get('life_duration') or 0)/1000, 'tick': (a.get('hit_speed') or 0)/1000,
             'air': bool(a.get('hits_air')), 'ground': bool(a.get('hits_ground')), 'own': bool(a.get('only_own_troops'))}
        if a.get('crown_tower_damage_percent'): s['ctd'] = 1 + a['crown_tower_damage_percent']/100
        if a.get('buff'):
            bdat = buff(a['buff']); s['buff'] = [a['buff'], (a.get('buff_time') or 0)/1000, bdat]
            if bdat['dps']: s['dps'] = round(bdat['dps'] * 1.1**idx(rar))
        if a.get('spawn_character'):
            s['kind'] = 'spawner'; s['spawn'] = {'unit': unit(a['spawn_character'], rar), 'interval': a['spawn_interval']/1000, 'delay': (a.get('spawn_initial_delay') or 0)/1000, 'minR': px(a.get('spawn_min_radius'))}
        if a.get('clone'): s['kind'] = 'clone'
        if a.get('projectile'):
            p = pj.get(a['projectile']) or {}
            s['kind'] = 'lightning'; s['dmg'] = lvl(p,'damage',rar); s['hits'] = 3; s['ctd'] = 1 + (p.get('crown_tower_damage_percent') or -70)/100
        CARDS.append({**base, 'type':'spell', 'spell': s, 'splash': s['radius'], 'dmg': s['dmg'] or s.get('dps',0), 'targets': 'any' if s['air'] else 'ground'}); continue
    # projectile spells
    projname = {'fireball':'FireballSpell','arrows':'ArrowsSpell','rocket':'RocketSpell','goblin-barrel':'GoblinBarrelSpell','the-log':'LogProjectileRolling',
                'barbarian-barrel':'BarbLogProjectileRolling','giant-snowball':'SnowballSpell','royal-delivery':'RoyalDeliveryProjectile'}.get(c['key'])
    p = pj.get(projname) if projname else None
    if not p: skipped.append(c['key']); continue
    s = {'kind':'projectile', 'travel': round(p.get('speed',600)/60*GW), 'radius': px(p.get('radius') or p.get('projectile_radius')),
         'dmg': lvl(p,'damage',rar), 'ctd': 1 + (p.get('crown_tower_damage_percent') or 0)/100, 'push': px(p.get('pushback')),
         'air': p.get('aoe_to_air', True) is not False, 'ground': True}
    if c['key'] == 'arrows': s['waves'] = 3
    if p.get('target_buff'): s['buff'] = [p['target_buff'], (p.get('buff_time') or 0)/1000, buff(p['target_buff'])]
    if p.get('projectile_range'):
        s['kind'] = 'roll'; s['roll'] = px(p['projectile_range']); s['halfW'] = px(p.get('projectile_radius')); s['air'] = False
        s['travel'] = round(p.get('speed',200)/60*GW*3.5)
    sc = p.get('spawn_character') or (pj.get('GoblinBarrel') or {}).get('spawn_character') if c['key']=='goblin-barrel' else p.get('spawn_character')
    if c['key'] == 'barbarian-barrel': sc = 'Barbarian'
    if c['key'] == 'royal-delivery': sc = 'DeliveryRecruit'
    if sc:
        s['spawn'] = [unit(sc, rar), p.get('spawn_character_count') or p.get('spawn_count') or (3 if c['key']=='goblin-barrel' else 1)]
    if c['key'] == 'royal-delivery': s['delay'] = 2.0; s['travel'] = 0
    CARDS.append({**base, 'type':'spell', 'spell': s, 'splash': s['radius'] or s.get('halfW',0), 'dmg': s['dmg'] * s.get('waves',1), 'targets': 'any' if s['air'] else 'ground'})

UNITS = {k:v for k,v in UNITS.items() if v}
for c in CARDS:
    if c['type']=='troop': c['units'] = [u for u in c['units'] if u[0] in UNITS]
CARDS = [c for c in CARDS if c['type']!='troop' or c['units']]
js = 'const UNITS = ' + json.dumps(UNITS, ensure_ascii=False, separators=(',',':')) + ';\n'
js += 'const ALL_CARDS = [\n' + ',\n'.join('  ' + json.dumps(c, ensure_ascii=False, separators=(',',':')) for c in CARDS) + '\n];\n'
open('/tmp/cr-gen/cards.js','w').write(js)
json.dump({'cards':CARDS,'units':UNITS,'skipped':skipped}, open('/tmp/cr-gen/out.json','w'), ensure_ascii=False, indent=0)
print('cards', len(CARDS), 'units', len(UNITS), 'skipped', skipped)
print('missing icons', [c['id'] for c in CARDS if c['icon']=='❔'])
for k in ['Knight','FireSpirits','ElectroSpirit','Witch','Xbow','InfernoTower','Golem','Tesla','Prince','Assassin','IceSpirits','HealSpirit','MegaKnight']:
    print(k, UNITS.get(k))
for c in CARDS:
    if c['id'] in ('goblin-gang','fire-spirit','fireball','the-log','zap','poison','graveyard','goblin-barrel','freeze','earthquake','tornado','rage','arrows','lightning','royal-delivery','barbarian-barrel','clone','giant-snowball','three-musketeers'):
        print(c['id'], c.get('units') or c.get('spell'))
