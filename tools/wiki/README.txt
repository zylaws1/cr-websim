Wiki balance check (current game values; RoyaleAPI data stops at Dec 2023)
1. python3 tools/gen_cards.py        -> /tmp/cr-gen/out.json
2. python3 tools/wiki/scrape.py      -> caches wiki pages via clashroyale.fandom.com/api.php (normal pages are blocked)
3. python3 tools/wiki/compare.py     -> /tmp/cr-wiki/diff.json, prints every mismatch
4. Review mismatches (spawner buildings' wiki stats describe the spawned unit — skip those), update
   tools/wiki/overrides.json and the WIKI_OVERRIDES block in index.html.
