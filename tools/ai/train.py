"""Self-play PPO trainer. Rollouts run in Node (tools/ai/rollout.mjs, all cores); updates run here in PyTorch.
usage: python3 tools/ai/train.py [--iters 300] [--matches 96] [--run ppo1]   (output in ~/cr-ai-runs/<run>, or $CR_RUNS)"""
import argparse, json, os, subprocess, sys, time, shutil, numpy as np, torch
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model import Net, export, OBS, CARD, TILES, KINDS, HAND

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
ap = argparse.ArgumentParser()
ap.add_argument('--iters', type=int, default=300); ap.add_argument('--matches', type=int, default=96)
ap.add_argument('--run', default='ppo1'); ap.add_argument('--lr', type=float, default=3e-4)
ap.add_argument('--resume', action='store_true'); ap.add_argument('--device', default='cpu')
ap.add_argument('--init', default=''); ap.add_argument('--kl', type=float, default=0.02); ap.add_argument('--bench-every', type=int, default=10)
ap.add_argument('--rollback', type=float, default=0.06); ap.add_argument('--vwarm', type=int, default=0); ap.add_argument('--ent', type=float, default=0.01)
args = ap.parse_args()
# training output lives outside the repo (the repo is in OneDrive, which would sync gigabytes of rollout data)
RUNS = os.environ.get('CR_RUNS', os.path.expanduser('~/cr-ai-runs'))
RUN = args.run if os.path.isabs(args.run) else os.path.join(RUNS, args.run.replace('tools/ai/runs/', '')); os.makedirs(RUN, exist_ok=True)
dev = torch.device(args.device); torch.set_num_threads(8)
net = Net().to(dev); opt = torch.optim.Adam(net.parameters(), lr=args.lr, eps=1e-5)
state = {'iter': 0, 'snapshots': [], 'phase': 'rule', 'log': []}
if args.resume and os.path.exists(os.path.join(RUN, 'state.json')):
    state = json.load(open(os.path.join(RUN, 'state.json')))
    ck = torch.load(os.path.join(RUN, 'last.pt')); net.load_state_dict(ck['net']); opt.load_state_dict(ck['opt'])
elif args.init:
    net.load_state_dict(torch.load(args.init)['net'])

GAMMA, LAM, CLIP, ENT, VF, EPOCHS, MB = 0.995, 0.95, 0.2, args.ent, 0.5, 4, 2048

def rollout(weights, out, n, opp, rule_share, greedy=0, seed=1):
    cmd = ['node', os.path.join(ROOT, 'tools/ai/rollout.mjs'), weights, out, str(n), opp, str(rule_share), str(greedy), str(seed)]
    r = subprocess.run(cmd, capture_output=True, text=True, cwd=ROOT)
    if r.returncode: raise RuntimeError(r.stderr[-3000:])
    return json.loads(r.stdout.strip().splitlines()[-1])

def load(d, st):
    f = lambda k, t: np.fromfile(os.path.join(d, k + '.bin'), dtype=t)
    n = st['n']
    return dict(obs=f('obs', np.float32).reshape(n, OBS), km=f('km', np.uint8).reshape(n, KINDS), tm=f('tm', np.uint8).reshape(n, TILES),
                kind=f('kind', np.int32), tile=f('tile', np.int32), logp=f('logp', np.float32), val=f('val', np.float32),
                rew=f('rew', np.float32), done=f('done', np.uint8))

def gae(rew, val, done):
    adv = np.zeros_like(rew); last = 0.0
    for t in range(len(rew) - 1, -1, -1):
        nv = 0.0 if done[t] else val[t + 1]
        if done[t]: last = 0.0
        delta = rew[t] + GAMMA * nv - val[t]
        last = delta + GAMMA * LAM * last; adv[t] = last
    return adv, adv + val

def logp_ent(B, idx):
    obs = B['obs'][idx]
    h, kl, v = net.trunk(obs)
    kl = kl.masked_fill(B['km'][idx] == 0, -1e9)
    lk = torch.log_softmax(kl, -1)
    kind = B['kind'][idx]
    lp = lk.gather(1, kind[:, None]).squeeze(1)
    ent = -(lk.exp() * lk).sum(-1)
    card = (kind >= 1) & (kind <= 4)
    if card.any():
        ci = card.nonzero().squeeze(1)
        k = kind[ci] - 1
        feat = torch.stack([obs[ci, HAND + CARD * kk: HAND + CARD * (kk + 1)] for kk in range(4)], 1)[torch.arange(len(ci)), k]
        tl = net.tile_logits(h[ci], feat).masked_fill(B['tm'][idx][ci] == 0, -1e9)
        lt = torch.log_softmax(tl, -1)
        lp = lp.index_add(0, ci, lt.gather(1, B['tile'][idx][ci][:, None]).squeeze(1))
        ent = ent.index_add(0, ci, -(lt.exp() * lt).sum(-1) * 0.2)
    return lp, ent, v

def update(B, value_only=False, ent_coef=ENT):
    adv, ret = gae(B['rew'], B['val'], B['done'])
    T = {k: torch.as_tensor(v, device=dev) for k, v in B.items()}
    T['kind'], T['tile'] = T['kind'].long(), T['tile'].long()
    A = torch.as_tensor((adv - adv.mean()) / (adv.std() + 1e-8), device=dev, dtype=torch.float32)
    R = torch.as_tensor(ret, device=dev, dtype=torch.float32)
    n, stats, stop = len(adv), [], False
    for _ in range(EPOCHS):
        if stop: break
        perm = torch.randperm(n, device=dev)
        for s in range(0, n, MB):
            idx = perm[s:s + MB]
            lp, ent, v = logp_ent(T, idx)
            ratio = (lp - T['logp'][idx]).exp()
            pg = -torch.min(ratio * A[idx], ratio.clamp(1 - CLIP, 1 + CLIP) * A[idx]).mean()
            vl = ((v - R[idx]) ** 2).mean()
            loss = VF * vl if value_only else pg + VF * vl - ent_coef * ent.mean()
            kl = (T['logp'][idx] - lp).mean().item()
            if not value_only and kl > 1.5 * args.kl: stop = True; break      # cap how far one update moves the policy
            opt.zero_grad(); loss.backward(); torch.nn.utils.clip_grad_norm_(net.parameters(), 0.5); opt.step()
            stats.append((pg.item(), vl.item(), ent.mean().item(), ((ratio - 1).abs() > CLIP).float().mean().item(), kl))
    return (np.mean(stats, 0) if stats else np.zeros(5)), float(np.mean(B['rew'][B['done'] == 1]))

def save():
    torch.save({'net': net.state_dict(), 'opt': opt.state_dict()}, os.path.join(RUN, 'last.pt'))
    json.dump(state, open(os.path.join(RUN, 'state.json'), 'w'))

W = os.path.join(RUN, 'cur'); B_DIR = os.path.join(RUN, 'batch')
START, T_START = state['iter'], time.time()
print(f"Training {args.run}: iterations {START + 1}-{args.iters}, {args.matches} matches each, {os.environ.get('CR_THREADS', os.cpu_count())} cores", flush=True)
while state['iter'] < args.iters:
    it = state['iter']; t0 = time.time()
    export(net, W)
    if state['phase'] == 'rule': opp, share = 'rule', 1.0
    else: opp, share = ','.join(state['snapshots'][-6:]) or 'self', 0.3
    st = rollout(W, B_DIR, args.matches, opp, share, seed=it + 1)
    t1 = time.time()
    B = load(B_DIR, st)
    ent_coef = ENT * (1 - 0.8 * it / max(1, args.iters))                      # explore less as training goes on
    (pg, vl, ent, clipf, kl), term = update(B, value_only=it < args.vwarm, ent_coef=ent_coef)
    wr = st['vsRule'][0] / max(1, st['vsRule'][1]); ws = st['vsSelf'][0] / max(1, st['vsSelf'][1])
    rec = dict(iter=it, samples=st['n'], vsRule=round(wr, 3), nRule=st['vsRule'][1], vsSnap=round(ws, 3), nSnap=st['vsSelf'][1],
               cardsPerMatch=round(st['cards'] / max(1, st['matches']), 1), pg=round(pg, 4), vl=round(vl, 4), ent=round(ent, 3), clip=round(clipf, 3), kl=round(float(kl), 4),
               roll_s=round(t1 - t0, 1), upd_s=round(time.time() - t1, 1), phase=state['phase'])
    state['log'].append(rec)
    with open(os.path.join(RUN, 'log.jsonl'), 'a') as lf: lf.write(json.dumps(rec) + '\n')
    # human-readable progress line
    last10 = [r['vsRule'] for r in state['log'][-10:] if r['nRule'] >= 10]
    secs = time.time() - t0; done_n = it + 1 - START; left = args.iters - it - 1
    avg_s = (time.time() - T_START) / max(1, done_n)
    eta = left * avg_s
    bar = '#' * int(20 * (it + 1) / args.iters) + '-' * (20 - int(20 * (it + 1) / args.iters))
    vs_snap = f" | vs past selves {rec['vsSnap']:.0%}" if rec['nSnap'] else ''
    print(f"[{bar}] iter {it + 1}/{args.iters} | vs rule AI {rec['vsRule']:.0%} (last 10 avg {np.mean(last10):.0%}){vs_snap} | "
          f"phase {state['phase']} | {secs:.1f}s/iter | ETA {int(eta // 60)}m {int(eta % 60):02d}s", flush=True)
    state['iter'] += 1
    # curriculum: once it beats the rule AI, add self-play against frozen snapshots
    recent = [r['vsRule'] for r in state['log'][-5:] if r['nRule'] >= 20]
    if state['phase'] == 'rule' and len(recent) == 5 and np.mean(recent) >= 0.6: state['phase'] = 'league'
    if state['iter'] % args.bench_every == 0:
        export(net, W)
        r = subprocess.run(['node', os.path.join(ROOT, 'tools/ai/bench.mjs'), W], capture_output=True, text=True, cwd=ROOT)
        bw = json.loads(r.stdout.strip().splitlines()[-1])['winRate'] if r.returncode == 0 else 0.0
        best = state.get('best', -1.0)
        msg = ''
        if bw > best:
            state['best'], state['best_iter'] = bw, state['iter']
            torch.save({'net': net.state_dict(), 'opt': opt.state_dict()}, os.path.join(RUN, 'best.pt')); msg = 'new best'
        elif bw < best - args.rollback:
            ck = torch.load(os.path.join(RUN, 'best.pt')); net.load_state_dict(ck['net']); opt.load_state_dict(ck['opt'])
            msg = f'rolled back to iter {state["best_iter"]}'
        state.setdefault('bench', []).append([state['iter'], bw])
        print(f"   ↳ benchmark (240 fixed matches vs rule AI, all 30 decks): {bw:.1%} | best {state['best']:.1%} @ iter {state['best_iter']} {msg}", flush=True)
    if state['iter'] % 10 == 0:
        snap = os.path.join(RUN, f'snap{state["iter"]:04d}')
        export(net, snap); state['snapshots'].append(snap)
    save()
