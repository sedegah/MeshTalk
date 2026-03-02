#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

echo "[1/4] Building MeshTalk C++ client/server..."
make

echo "[2/4] Preparing Cloudflare backend dependencies..."
if command -v npm >/dev/null 2>&1; then
  if ! (cd cloudflare-backend && npm install); then
    echo "WARNING: npm install failed (likely network/policy restrictions); continuing"
  fi
else
  echo "WARNING: npm not found; skipping cloudflare-backend dependency install"
fi

echo "[3/4] Checking backend syntax..."
if command -v node >/dev/null 2>&1; then
  node --check cloudflare-backend/src/index.js
else
  echo "WARNING: node not found; skipping JS syntax check"
fi

echo "[4/4] Setup complete."
echo "Run './server' in one terminal and './client 127.0.0.1' in another to test local chat."
echo "For cloud backend, configure KV IDs in cloudflare-backend/wrangler.toml then run 'npm run dev' in cloudflare-backend/."
