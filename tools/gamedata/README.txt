Live game data (client 16.402.19, content update 2026-10-06), decoded from Supercell's csv_logic tables by
RoyaleGym/RoyaleSim (https://github.com/RoyaleGym/RoyaleSim, MIT — see LICENSE-RoyaleSim).
Stats are level 1; level 11 = 256% (floor) for every rarity.

Refresh: dump the page's UNITS (see convert.py docstring) or reuse unit_ids.json, then
  python3 tools/gamedata/convert.py [live_units.json]   -> overrides.json (+ diff report on stderr)
and paste overrides.json over `const GAME_DATA = ...` in index.html. KEEP/SKIP in convert.py list values held back
(Spirit Empress HP unconfirmed, X-Bow range calibrated to user screenshots, reworked units handled by hand).
