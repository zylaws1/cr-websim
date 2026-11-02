"""Expert iteration: the look-ahead search plays, the network learns to imitate its moves, repeat with the better network.
usage: CR_THREADS=8 python3 tools/ai/expert_iter.py [--rounds 3] [--matches 1200] [--init tools/ai/models/v2-ppo200.pt]
Output: ~/cr-ai-runs/expert/round<N>/; the best round is copied to tools/ai/models/ and exported to ai/model.js."""
import argparse, json, os, shutil, subprocess, sys, time, numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
RUNS_ROOT = os.environ.get('CR_RUNS', os.path.expanduser('~/cr-ai-runs'))
ap = argparse.ArgumentParser()
ap.add_argument('--rounds', type=int, default=3); ap.add_argument('--matches', type=int, default=1200)
ap.add_argument('--init', default='tools/ai/models/v2-ppo200.pt'); ap.add_argument('--epochs', type=int, default=4)
ap.add_argument('--bench', type=int, default=240)
ap.add_argument('--selfplay', action='store_true', help='search vs search games; promote a network only if it beats the champion head-to-head')
ap.add_argument('--run', default='')
args = ap.parse_args()
RUNS = os.path.join(RUNS_ROOT, args.run or ('expert_sp' if args.selfplay else 'expert'))
os.makedirs(RUNS, exist_ok=True)

def sh(cmd, env=None, quiet=False):
    r = subprocess.run(cmd, cwd=ROOT, env={**os.environ, **(env or {})}, text=True, stdout=subprocess.PIPE, stderr=None if not quiet else subprocess.DEVNULL)
    if r.returncode: raise SystemExit(f'failed: {" ".join(cmd)}')
    return r.stdout.strip().splitlines()[-1]

def bench(agent, model_js):
    out = json.loads(sh(['node', 'tools/ai/bench_agents.mjs', agent, 'rule', str(args.bench)], env={'CR_MODEL': model_js}, quiet=True))
    return out['winRate'], out['ci95']

def h2h(cand_js, champ_js):
    """candidate network vs champion network, both with look-ahead, paired fixed matches → candidate win rate"""
    out = json.loads(sh(['node', 'tools/ai/bench_agents.mjs', 'search', 'search', str(args.bench), '{"net":"B"}', '{}'],
                        env={'CR_MODEL': champ_js, 'CR_MODEL_B': cand_js}, quiet=True))
    return out['winRate'], out['ci95']

def merge(dirs, out):
    """concatenate all rounds' expert data (DAgger-style: keep every state the search was asked about)"""
    metas = [json.load(open(os.path.join(d, 'meta.json'))) for d in dirs]
    n = sum(m['n'] for m in metas); os.makedirs(out, exist_ok=True)
    for k in ['obs', 'km', 'tm', 'kind', 'tile']:
        with open(os.path.join(out, k + '.bin'), 'wb') as f:
            for d in dirs: f.write(open(os.path.join(d, k + '.bin'), 'rb').read())
    counts = np.sum([m['counts'] for m in metas], 0).tolist()
    json.dump({'n': n, 'counts': counts}, open(os.path.join(out, 'meta.json'), 'w'))
    return n

state_f = os.path.join(RUNS, 'state.json')
state = json.load(open(state_f)) if os.path.exists(state_f) else {'round': 0, 'ckpt': os.path.join(ROOT, args.init), 'data': [], 'history': [], 'best': None}
if args.selfplay and not state['history']:
    js = os.path.join(RUNS, 'model_r0.js'); sh(['python3', 'tools/ai/export_browser.py', state['ckpt'], js])
    state['history'].append({'round': 0, 'champion': True}); state['best'] = {'round': 0, 'ckpt': state['ckpt'], 'js': js, 'search': 0.5}
    print(f"self-play expert iteration from {state['ckpt']} (champion = round 0)", flush=True)
    json.dump(state, open(state_f, 'w'))
if not state['history']:
    js = os.path.join(RUNS, 'model_r0.js'); sh(['python3', 'tools/ai/export_browser.py', state['ckpt'], js])
    t = time.time(); nn = bench('nn', js); se = bench('search', js)
    state['history'].append({'round': 0, 'nn': nn[0], 'search': se[0]}); state['best'] = {'round': 0, 'ckpt': state['ckpt'], 'js': js, 'search': se[0]}
    print(f"round 0 (start, {state['ckpt']}): network alone {nn[0]:.1%} ±{nn[1]:.0%} | + look-ahead {se[0]:.1%} ±{se[1]:.0%} vs rule AI  ({time.time() - t:.0f}s)", flush=True)
    json.dump(state, open(state_f, 'w'))

for _ in range(args.rounds):
    r = state['round'] + 1; rd = os.path.join(RUNS, f'round{r}'); os.makedirs(rd, exist_ok=True)
    cur_js = state['best']['js'] if args.selfplay else os.path.join(RUNS, f'model_r{r - 1}.js')
    print(f"\n=== round {r}: 1) search plays {args.matches} matches with the round-{r - 1} network", flush=True)
    t = time.time(); info = json.loads(sh(['node', 'tools/ai/expert_collect.mjs', os.path.join(rd, 'data'), str(args.matches), '1.0' if args.selfplay else '0.5'], env={'CR_MODEL': cur_js}))
    state['data'].append(os.path.join(rd, 'data'))
    vr = f", search won {info['searchVsRuleWin']:.0%} of its games vs the rule AI" if info['searchVsRuleWin'] is not None else ''
    print(f"    {info['n']} search decisions recorded{vr} ({time.time() - t:.0f}s)", flush=True)
    print(f"=== round {r}: 2) train the network on all {len(state['data'])} round(s) of search decisions", flush=True)
    n = merge(state['data'], os.path.join(RUNS, 'merged'))
    t = time.time()
    p = subprocess.run(['python3', 'tools/ai/bc_train.py', os.path.join(RUNS, 'merged'), rd, str(args.epochs), state['ckpt'], '3e-4'], cwd=ROOT,
                       env={**os.environ, 'CR_PLAYW': '1', **({'CR_NOWAIT': '1'} if args.selfplay else {})}, text=True, capture_output=True)
    if p.returncode: raise SystemExit(p.stderr[-2000:])
    last = json.loads(p.stdout.strip().splitlines()[-1])
    print(f"    {n} samples, {args.epochs} epochs: imitates the search's card choice {last['kind_acc']:.0%}, tile within 1 tile {last['tile_acc_1']:.0%} ({time.time() - t:.0f}s)", flush=True)
    ck = os.path.join(rd, 'bc.pt'); js = os.path.join(RUNS, f'model_r{r}.js'); sh(['python3', 'tools/ai/export_browser.py', ck, js])
    if args.selfplay:
        print(f"=== round {r}: 3) head-to-head: new network vs champion (round {state['best']['round']}), both with look-ahead, {args.bench} paired matches", flush=True)
        t = time.time(); wr, ci = h2h(js, state['best']['js'])
        print(f"    new network wins {wr:.1%} ±{ci:.0%} ({time.time() - t:.0f}s)", flush=True)
        state['history'].append({'round': r, 'h2h': wr, 'samples': n}); state['ckpt'] = ck; state['round'] = r
        if wr > 0.52:
            state['best'] = {'round': r, 'ckpt': ck, 'js': js, 'search': wr}; print(f"    ★ promoted: round {r} is the new champion", flush=True)
        else:
            state['ckpt'] = state['best']['ckpt']; print(f"    kept the champion (round {state['best']['round']}); next round trains from it again", flush=True)
        json.dump(state, open(state_f, 'w')); continue
    print(f"=== round {r}: 3) benchmark ({args.bench} fixed matches vs rule AI, all 30 decks)", flush=True)
    t = time.time(); nn = bench('nn', js); se = bench('search', js)
    prev = state['history'][-1]
    print(f"    network alone {nn[0]:.1%} ±{nn[1]:.0%} (was {prev['nn']:.1%}) | + look-ahead {se[0]:.1%} ±{se[1]:.0%} (was {prev['search']:.1%})  ({time.time() - t:.0f}s)", flush=True)
    state['history'].append({'round': r, 'nn': nn[0], 'search': se[0], 'samples': n})
    state['ckpt'] = ck; state['round'] = r
    if se[0] >= state['best']['search']:
        state['best'] = {'round': r, 'ckpt': ck, 'js': js, 'search': se[0]}; print(f"    ★ new best (round {r})", flush=True)
    json.dump(state, open(state_f, 'w'))

b = state['best']
shutil.copy(b['js'], os.path.join(ROOT, 'ai/model.js'))
tag = 'sp' if args.selfplay else 'r'
if not args.selfplay or b['round'] > 0:
    shutil.copy(b['ckpt'], os.path.join(ROOT, 'tools/ai/models', f"expert-{tag}{b['round']}.pt"))
print(f"\nchampion: round {b['round']}" + (f" (beat the previous champion {b['search']:.1%})" if args.selfplay and b['round'] else '') + " — installed in ai/model.js", flush=True)
