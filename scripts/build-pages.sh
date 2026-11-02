#!/usr/bin/env bash
# Builds the public site for Cloudflare Pages (cr.vber-ai.com): only the game files, never tools/ (dev data, training checkpoints, scripts).
# Pages settings: build command `bash scripts/build-pages.sh`, output directory `site`.
set -euo pipefail
cd "$(dirname "$0")/.."
rm -rf site
mkdir site
cp -R index.html img sound ai LICENSE NOTICE site/          # ai/model.js = the trained AI (opponent + coach)
cp _headers site/_headers
find site -name '.DS_Store' -delete
echo "site/ ready: $(find site -type f | wc -l | tr -d ' ') files, $(du -sh site | cut -f1)"
