#!/usr/bin/env bash
# Removes all upstream GitHub Actions workflows from a local clone of the
# VTSTech/DevilutionX fork, keeping ONLY gpu-shaders.yml (our GPU shader
# scaler CI). Run from the repo root, then commit + push.
set -euo pipefail

cd "$(dirname "$0")"
if [ ! -d .github/workflows ]; then
    echo "error: run this from inside the repository (or keep it next to .github)" >&2
    exit 1
fi

KEEP=".github/workflows/gpu-shaders.yml"
removed=0
for f in .github/workflows/*.yml .github/workflows/*.yaml; do
    [ -e "$f" ] || continue
    if [ "$f" = "$KEEP" ]; then
        echo "keep    $f"
        continue
    fi
    if git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
        git rm -q "$f"
    else
        rm -f "$f"
    fi
    echo "removed $f"
    removed=$((removed + 1))
done

echo
echo "Done. Removed $removed workflow file(s)."
if git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    echo "Now commit and push:"
    echo "    git commit -m 'CI: remove upstream workflows, keep GPU shader scaler pipeline'"
    echo "    git push"
fi
