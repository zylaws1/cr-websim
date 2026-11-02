#!/bin/bash
# Head-to-head tuning: each challenger search setting plays 240 fixed matches against the champion (default SEARCH).
# >50% means the challenger is stronger. usage: CR_THREADS=8 tools/ai/tune_search.sh '<json>' '<json>' ...   (log: ~/cr-ai-runs/tune.log)
cd "$(dirname "$0")/../.."
LOG=~/cr-ai-runs/tune.log
i=0; n=$#
for cfg in "$@"; do
  i=$((i+1)); t0=$(date +%s)
  echo "[$i/$n] challenger $cfg vs champion (default) — running 240 matches…" | tee -a "$LOG"
  r=$(node tools/ai/bench_agents.mjs search search 240 "$cfg" '{}' 2>/dev/null)
  wr=$(echo "$r" | sed -E 's/.*"winRate":([0-9.]+).*"ci95":([0-9.]+).*/\1 \2/')
  set -- $wr
  printf "      → challenger wins %.1f%% ±%.1f%%  (%ds)\n" "$(echo "$1*100" | bc -l)" "$(echo "$2*100" | bc -l)" $(( $(date +%s) - t0 )) | tee -a "$LOG"
  echo "{\"cfg\":$cfg,\"res\":$r}" >> ~/cr-ai-runs/tune.jsonl
done
