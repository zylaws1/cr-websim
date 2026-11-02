import json, re, sys
sys.argv = ['x']
src = open('/tmp/cr-wiki/scrape.py').read()
exec(src.split("res = {}")[0])
out = json.load(open('/tmp/cr-gen/out.json')); U = out['units']
G = 400 / 18
num = lambda s: float(re.findall(r'[\d.]+', s.replace(',', ''))[0]) if s and re.findall(r'[\d.]+', s.replace(',', '')) else None
rows = []
for c in out['cards']:
    title = TITLE.get(c['id'], c['name'])
    d = fetch(title)
    if 'parse' not in d: rows.append((c['id'], 'NO PAGE', '', '')); continue
    w = d['parse']['wikitext']['*']
    V = {k.strip(): v.strip() for k, v in re.findall(r'\{\{#vardefine:\s*([^|]+)\|\s*([^}]*)\}\}', w)}
    A = {}
    for heads, rs in tables(w):
        if rs and ('Hit Speed' in heads or 'Cost' in heads):
            A = dict(zip(heads, rs[0])); break
    def chk(field, ours, wiki, tol=0.02):
        if wiki is None or ours is None: return
        if abs(ours - wiki) > max(tol * abs(wiki), 0.051): rows.append((c['id'], field, ours, wiki))
    chk('cost', c['cost'], num(A.get('Cost', '')), 0)
    main = U.get((c.get('units') or [[c.get('unit')]])[0][0]) if c['type'] != 'spell' else None
    if main:
        chk('hp_11', main.get('hp'), num(V.get('hp_11', '')))
        dmg_w = num(V.get('dmg_11', '')) or num(V.get('area_dmg_11', ''))
        chk('dmg_11', main.get('dmg'), dmg_w)
        chk('hit_speed', main.get('atk'), num(A.get('Hit Speed', '')))
        sp = re.findall(r'\((\d+)\)', A.get('Speed', ''))
        if sp: chk('speed(/min)', round(main.get('spd', 0) * 60), float(sp[0]))
        chk('deploy', main.get('deploy'), num(A.get('Deploy Time', '')))
        rg = A.get('Range', '')
        rn = re.findall(r'\(([\d.]+)\)', rg) or re.findall(r'[\d.]+', rg)
        if rn and c['id'] != 'x-bow': chk('range(tiles)', round(main.get('range', 0) / G, 2), float(rn[0]))
        tg = A.get('Target', '') or A.get('Targets', '')
        if tg:
            wt = 'building' if 'Building' in tg else 'any' if ('Air' in tg and 'Ground' in tg) else 'air' if 'Air' in tg else 'ground'
            if wt != main.get('targets'): rows.append((c['id'], 'targets', main.get('targets'), tg))
        tr_ = A.get('Transport', '')
        if tr_ and (('Air' in tr_) != bool(main.get('air'))): rows.append((c['id'], 'air', bool(main.get('air')), tr_))
        cnt = A.get('Count', '')
        if cnt and c['type'] == 'troop':
            wc = sum(int(x) for x in re.findall(r'x\s*(\d+)', cnt)) or None
            oc = sum(n for _, n in c['units'])
            if wc and wc != oc: rows.append((c['id'], 'count', oc, cnt))
        lt = A.get('Lifetime', '')
        if lt and main.get('life'): chk('lifetime', main.get('life'), num(lt))
    else:
        s = c.get('spell', {})
        dw = num(V.get('dmg_11', '')) or num(V.get('area_dmg_11', ''))
        if dw and s.get('dmg'): chk('spell dmg_11', s['dmg'], dw)
        rad = A.get('Radius', '')
        if rad and s.get('radius'): chk('radius(tiles)', round(s['radius'] / G, 2), num(rad))
json.dump(rows, open('/tmp/cr-wiki/diff.json', 'w'))
from collections import Counter
print('mismatches:', len(rows), Counter(r[1] for r in rows))
for r in rows: print(r)
