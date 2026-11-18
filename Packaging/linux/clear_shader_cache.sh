#!/usr/bin/env bash
# Written by VTSTech (Nigel Todman) (veritas@vts-tech.org)
# AI Assisted by Super-Z (GLM-5.3-Flash)
# License: MIT

# ============================================================
#  clear_shader_cache.sh - delete shaders/cache/, the
#  translated-shader compile cache. Linux equivalent of
#  clear_shader_cache.cmd. The cache is rebuilt automatically
#  on the next preset load; run this after a corpus update or
#  a game update if presets misbehave.
# ============================================================

# Operate next to this script (the scripts ship beside the
# game binary), not in the caller's working directory.
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)" || exit 1
cd "$SCRIPT_DIR" || exit 1

echo "Clearing shaders cache..."
rm -rf ./shaders/cache
echo "Shader cache cleared"
exit 0
