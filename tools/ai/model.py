"""Policy/value network shared with tools/ai/policy.mjs (same layer names, shapes and order)."""
import json, numpy as np, torch, torch.nn as nn

OBS, CARD, TILES, KINDS, H = 1454, 18, 540, 6, 256
HAND = OBS - 5 * CARD          # offset of the hand-card features in the observation
LAYERS = ['t1', 't2', 'kind', 'val', 'p1', 'p2']

class Net(nn.Module):
    def __init__(self):
        super().__init__()
        self.t1, self.t2 = nn.Linear(OBS, H), nn.Linear(H, H)
        self.kind, self.val = nn.Linear(H, KINDS), nn.Linear(H, 1)
        self.p1, self.p2 = nn.Linear(H + CARD, H), nn.Linear(H, TILES)
        for m in [self.kind, self.p2]: nn.init.orthogonal_(m.weight, 0.01); nn.init.zeros_(m.bias)
        nn.init.orthogonal_(self.val.weight, 1.0)
    def trunk(self, obs):
        h = torch.relu(self.t2(torch.relu(self.t1(obs))))
        return h, self.kind(h), self.val(h).squeeze(-1)
    def tile_logits(self, h, card):
        return self.p2(torch.relu(self.p1(torch.cat([h, card], -1))))

def export(net, base):
    shapes, parts = {}, []
    for name in LAYERS:
        L = getattr(net, name)
        w, b = L.weight.detach().cpu().float().numpy(), L.bias.detach().cpu().float().numpy()
        shapes[name] = list(w.shape); parts += [w.ravel(), b.ravel()]
    np.concatenate(parts).astype(np.float32).tofile(base + '.bin')
    json.dump({'shapes': shapes, 'obs': OBS}, open(base + '.json', 'w'))
