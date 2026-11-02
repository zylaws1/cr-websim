import json, re, urllib.request, urllib.parse, time, os, sys
cards = json.load(open('/tmp/cr-gen/out.json'))['cards']
TITLE = {'pekka': 'P.E.K.K.A', 'mini-pekka': 'Mini P.E.K.K.A', 'x-bow': 'X-Bow', 'the-log': 'The Log'}
def fetch(title):
    fn = '/tmp/cr-wiki/' + re.sub(r'[^A-Za-z0-9]', '_', title) + '.json'
    if os.path.exists(fn): return json.load(open(fn))
    url = 'https://clashroyale.fandom.com/api.php?' + urllib.parse.urlencode({'action': 'parse', 'page': title, 'prop': 'wikitext', 'format': 'json', 'redirects': 1})
    req = urllib.request.Request(url, headers={'User-Agent': 'Mozilla/5.0'})
    try: d = json.load(urllib.request.urlopen(req, timeout=25))
    except Exception as e: d = {'error': str(e)}
    json.dump(d, open(fn, 'w')); time.sleep(0.3)
    return d
def clean(c):
    c = re.sub(r'\{\{Rarity\|([^}|]+)[^}]*\}\}', r'\1', c)
    c = re.sub(r'\[\[(?:[^]|]*\|)?([^]]*)\]\]', r'\1', c)
    c = re.sub(r'<[^>]+>', ' ', c)
    return re.sub(r'\s+', ' ', c).strip()
def tables(w):
    out = []
    for m in re.finditer(r'\{\|(.*?)\n\|\}', w, re.S):
        t = m.group(1)
        heads = [clean(re.sub(r'\{\{Icon[^}]*\}\}', '', h).split('|')[-1]) for h in re.findall(r'^!(.*)$', t, re.M)]
        rows = []
        for r in re.split(r'\n\|-', t)[1:]:
            body = '\n'.join(l for l in r.split('\n') if l.startswith('|') and not l.startswith('|}'))
            cells = [clean(x) for x in re.split(r'\|\||\n\|', body.lstrip('|'))]
            if any(cells): rows.append(cells)
        out.append((heads, rows))
    return out
res = {}
for c in cards:
    title = TITLE.get(c['id'], c['name'])
    d = fetch(title)
    if 'parse' not in d: res[c['id']] = {'error': d.get('error', 'missing')}; continue
    w = d['parse']['wikitext']['*']
    info = {'attrs': {}, 'l11': {}}
    for heads, rows in tables(w):
        if not rows: continue
        if 'Hit Speed' in ' '.join(heads) or 'Cost' in heads or 'Range' in heads:
            if not info['attrs']:
                info['attrs'] = dict(zip(heads, rows[0]))
        if heads and heads[0].startswith('Level'):
            for r in rows:
                if r and r[0].strip() == '11': info['l11'] = dict(zip(heads, r)); break
    res[c['id']] = info
json.dump(res, open('/tmp/cr-wiki/wiki.json', 'w'), indent=1)
print(len(res), 'errors:', [k for k, v in res.items() if 'error' in v])
print(json.dumps({k: res[k] for k in ['knight', 'x-bow', 'fireball', 'hunter']}, indent=1)[:2500])
