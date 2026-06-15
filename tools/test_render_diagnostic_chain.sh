#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

make -C "$ROOT_DIR/src/demos/d4" evidence-diagnostic

