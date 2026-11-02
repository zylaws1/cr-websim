#!/usr/bin/env bash
# Build site/ and upload it to Cloudflare Pages (free plan, direct upload) — project vber-cr → https://cr.vber-ai.com
# First time only: npx --registry=https://registry.npmjs.org wrangler login
set -euo pipefail
cd "$(dirname "$0")/.."
bash scripts/build-pages.sh
npx --yes --registry=https://registry.npmjs.org wrangler@latest pages deploy site --project-name "${CF_PAGES_PROJECT:-vber-cr}" --branch main --commit-dirty=true
