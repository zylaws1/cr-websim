"""Behaviour cloning: train the policy to imitate recorded decisions (rule AI, or the look-ahead search for expert iteration).
usage: python3 tools/ai/bc_train.py <dataDir> <outRunDir> [epochs=8] [initCheckpoint] [lr=1e-3]"""
import json, os, sys, numpy as np, torch
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from model import Net, export, OBS, CARD, TILES, KINDS, HAND
d, run = sys.argv[1], sys.argv[2]; epochs = int(sys.argv[3]) if len(sys.argv) > 3 else 8
os.makedirs(run, exist_ok=True)
meta = json.load(open(os.path.join(d, 'meta.json'))); n = meta['n']
f = lambda k, t: np.fromfile(os.path.join(d, k + '.bin'), dtype=t)
obs = torch.as_tensor(f('obs', np.float32).reshape(n, OBS)); km = torch.as_tensor(f('km', np.uint8).reshape(n, KINDS))
tm = torch.as_tensor(f('tm', np.uint8).reshape(n, TILES)); kind = torch.as_tensor(f('kind', np.int32)).long(); tile = torch.as_tensor(f('tile', np.int32)).long()
# re-weight: plays are rarer than waits
w = torch.where(kind == 0, torch.tensor(1.0), torch.tensor(float(os.environ.get('CR_PLAYW', '2'))))
if os.environ.get('CR_NOWAIT'):
    # learn WHICH card / tile the search prefers, not WHEN to play: drop 'wait' labels and exclude the wait class from the loss
    keep = (kind != 0).nonzero().squeeze(1)
    obs, km, tm, kind, tile, w = obs[keep], km[keep], tm[keep], kind[keep], tile[keep], w[keep]; n = len(kind)
    km = km.clone(); km[:, 0] = 0
torch.set_num_threads(12); net = Net()
if len(sys.argv) > 4 and sys.argv[4]: net.load_state_dict(torch.load(sys.argv[4])['net'])
opt = torch.optim.Adam(net.parameters(), lr=float(sys.argv[5]) if len(sys.argv) > 5 else 1e-3)
perm0 = torch.randperm(n); val_idx, tr_idx = perm0[: n // 20], perm0[n // 20:]
def losses(idx):
    h, kl, _ = net.trunk(obs[idx]); kl = kl.masked_fill(km[idx] == 0, -1e9)
    lk = torch.log_softmax(kl, -1); lkind = -(lk.gather(1, kind[idx][:, None]).squeeze(1) * w[idx]).mean()
    acc = (kl.argmax(-1) == kind[idx]).float().mean()
    c = ((kind[idx] >= 1) & (kind[idx] <= 4)).nonzero().squeeze(1)
    if len(c) == 0: return lkind, torch.tensor(0.0), acc, torch.tensor(0.0)
    ii = idx[c]; k = kind[ii] - 1
    feat = torch.stack([obs[ii, HAND + CARD * j: HAND + CARD * (j + 1)] for j in range(4)], 1)[torch.arange(len(ii)), k]
    tl = net.tile_logits(h[c], feat).masked_fill(tm[ii] == 0, -1e9)
    lt = torch.log_softmax(tl, -1); ltile = -lt.gather(1, tile[ii][:, None]).mean()
    # tile accuracy within 1 tile
    pc, pr = tl.argmax(-1) % 18, tl.argmax(-1) // 18; tc, trr = tile[ii] % 18, tile[ii] // 18
    tacc = (((pc - tc).abs() <= 1) & ((pr - trr).abs() <= 1)).float().mean()
    return lkind, ltile, acc, tacc
for ep in range(epochs):
    perm = tr_idx[torch.randperm(len(tr_idx))]
    for s in range(0, len(perm), 1024):
        lk, lt, _, _ = losses(perm[s:s + 1024]); loss = lk + lt
        opt.zero_grad(); loss.backward(); opt.step()
    with torch.no_grad():
        lk, lt, acc, tacc = losses(val_idx)
    print(json.dumps(dict(epoch=ep, kind_loss=round(lk.item(), 4), tile_loss=round(lt.item(), 4), kind_acc=round(acc.item(), 3), tile_acc_1=round(tacc.item(), 3))), flush=True)
torch.save({'net': net.state_dict()}, os.path.join(run, 'bc.pt')); export(net, os.path.join(run, 'bc'))
