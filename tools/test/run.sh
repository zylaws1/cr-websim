#!/bin/bash
# Headless test suite for index.html (no Playwright needed — uses the installed Google Chrome).
# usage: tools/test/run.sh [path/to/index.html]
# Injects harness.html.inc before </body>, runs every scenario via tick(), prints PASS/FAIL.
set -e
here="$(cd "$(dirname "$0")" && pwd)"
src="${1:-$here/../../index.html}"
work="$(mktemp -d /tmp/cr-test.XXXX)"
cp -R "$(dirname "$src")/img" "$work/img" 2>/dev/null || true
cp -R "$(dirname "$src")/ai" "$work/ai" 2>/dev/null || true
python3 - "$src" "$here/harness.html.inc" "$work/test.html" <<'PY'
import sys
s = open(sys.argv[1]).read(); h = open(sys.argv[2]).read()
i = s.rindex('</body>'); open(sys.argv[3], 'w').write(s[:i] + h + s[i:])
PY
chrome="/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"
"$chrome" --headless=new --disable-gpu --no-first-run --user-data-dir="$work/profile" --window-size=500,900 \
  --dump-dom "file://$work/test.html" 2>/dev/null > "$work/dom.html" &
pid=$!; ( sleep 420; kill $pid 2>/dev/null ) & wd=$!   # macOS has no `timeout`
wait $pid 2>/dev/null || true; kill $wd 2>/dev/null || true
python3 - "$work/dom.html" <<'PY'
import sys, re, json, html
m = re.search(r'<pre id="results">(.*?)</pre>', open(sys.argv[1]).read(), re.S)
if not m: print('NO RESULTS (page crashed or timed out)'); sys.exit(1)
d = json.loads(html.unescape(m.group(1)))
print('page errors:', d['errors'] or 'none'); f = 0
for r in d['results']:
    f += not r['pass']; print(('PASS ' if r['pass'] else 'FAIL ') + r['name'] + (f"  [{r['info']}]" if r['info'] != '' else ''))
print(f"{len(d['results']) - f}/{len(d['results'])} passed"); sys.exit(1 if f else 0)
PY
