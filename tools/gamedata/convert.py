"""Convert RoyaleGym/RoyaleSim's decoded client data (MIT, client 16.402.19, 2026-10-06) into level-11 overrides
for index.html's UNITS. usage: python3 tools/gamedata/convert.py [live_units.json]  -> prints the JS object; with a
live dump (UNITS from the page) also writes a diff report to stderr."""
import json, math, sys, os
here = os.path.dirname(os.path.abspath(__file__))
D = json.load(open(os.path.join(here, 'cards-160402017-20261006.json')))
GW = 400 / 18
L11 = 2.56                                    # every card: level 11 = 256% of the level-1 stats (floor)
# our unit id -> game unit name (where ours differ)
MAP = {'RuneGiant': 'GiantBuffer', 'SpiritEmpressAir': 'MergeMaiden_Mounted', 'SpiritEmpressGround': 'MergeMaiden_Normal',
       'Goblinstein': 'goblinstein_doctor', 'GoblinsteinMonster': 'goblinstein', 'LumberGhost': 'RageBarbarianEvoGhost',
       'Guardienne': 'ChampionGuard', 'Souldier': 'Ghost_EV1_Summon_Left', 'ThreeMusketeer': 'ThreeMusketeer_Rework_Character_1'}
# fields to keep from our own data (unconfirmed or user-calibrated), or units handled by hand in index.html
KEEP = {'SpiritEmpressAir': {'hp'}, 'SpiritEmpressGround': {'hp'},   # data 1121 HP vs wiki 1798: unconfirmed
        'LumberGhost': {'hp'}, 'Xbow': {'range', 'sight'},            # ghost is invulnerable; X-Bow range calibrated to user screenshots
        'SuspiciousBush': {'atk', 'range'}, 'MovingCannon': {'hp'}, 'BrokenCannon': {'hp'}}
SKIP = {'SuperHogRider_Terry', 'BattleHealer', 'GoblinPartyHut', 'GoblinBrawlerParty', 'SpearGoblinParty', 'DecoyGoblin',
        'GoblinHut', 'FirespiritHut'}
units = {**D['units']}
def px(milli): return None if milli is None else round(milli / 1000 * GW, 1)
def conv(gu):
    o = {}
    if gu.get('hitpoints'): o['hp'] = math.floor(gu['hitpoints'] * L11)
    if gu.get('damage'): o['dmg'] = math.floor(gu['damage'] * L11)
    if gu.get('shield_hitpoints'): o['shield'] = math.floor(gu['shield_hitpoints'] * L11)
    if gu.get('hit_speed_ms'): o['atk'] = gu['hit_speed_ms'] / 1000
    if gu.get('load_time_ms') is not None and gu.get('hit_speed_ms'): o['load'] = gu['load_time_ms'] / 1000
    if gu.get('speed'): o['spd'] = round(gu['speed'] / 60, 3)
    if gu.get('range_milli'): o['range'] = px(gu['range_milli'])
    if gu.get('minimum_range_milli'): o['minRange'] = px(gu['minimum_range_milli'])
    if gu.get('sight_range_milli'): o['sight'] = px(gu['sight_range_milli'])
    if gu.get('collision_radius_milli'): o['sz'] = round(2 * gu['collision_radius_milli'] / 1000 * GW)
    if gu.get('deploy_time_ms') is not None: o['deploy'] = gu['deploy_time_ms'] / 1000
    if gu.get('mass') is not None: o['mass'] = gu['mass']
    if gu.get('lifetime_ms'): o['life'] = gu['lifetime_ms'] / 1000
    if gu.get('area_damage_radius_milli'): o['splash'] = px(gu['area_damage_radius_milli'])
    p = gu.get('projectile')
    pr = D['projectiles'].get(p) if isinstance(p, str) else None
    if pr:
        if pr.get('speed'): o['projSpd'] = round(pr['speed'] / 60 * GW)
        if pr.get('radius_milli'): o['splash'] = px(pr['radius_milli'])
    if gu.get('crown_tower_damage_percent') not in (None, 100): o['ctd'] = gu['crown_tower_damage_percent'] / 100
    return o
def main():
    live = json.load(open(sys.argv[1]))['UNITS'] if len(sys.argv) > 1 else None
    ids = live or json.load(open(os.path.join(here, 'unit_ids.json')))
    out = {}
    for uid in ids:
        if uid in SKIP: continue
        g = units.get(MAP.get(uid, uid))
        if not g: print('no game data:', uid, file=sys.stderr); continue
        o = conv(g)
        for k in KEEP.get(uid, ()): o.pop(k, None)
        out[uid] = o
        if live:
            cur = live[uid]
            diffs = []
            for k, v in o.items():
                c = cur.get('proj', {}).get('spd') if k == 'projSpd' else cur.get(k)
                if c is None or (isinstance(v, (int, float)) and isinstance(c, (int, float)) and abs(v - c) <= max(0.02 * abs(c), 0.06)): continue
                diffs.append(f'{k} {c}->{v}')
            if diffs: print(f'{uid:22s} ' + ', '.join(diffs), file=sys.stderr)
    if live: json.dump(sorted(live), open(os.path.join(here, 'unit_ids.json'), 'w'))
    json.dump(out, open(os.path.join(here, 'overrides.json'), 'w'), separators=(',', ':'))
    print(json.dumps(out, separators=(',', ':')))
main()
