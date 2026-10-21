#!/usr/bin/env bash
set -euo pipefail

if [[ $# -eq 0 ]]; then
	>&2 echo "Usage: tools/update_bundled_assets.sh [path-to-devilutionx-assets-repo-dir]"
	exit 64
fi

ASSETS_REPO_DIR="$1"
OUTPUT_DIR="${PWD}/assets"

set -x
cd "${ASSETS_REPO_DIR}/bundled-assets"
pcx2clx --output-dir "${OUTPUT_DIR}/data" data/boxleftend.pcx data/boxmiddle.pcx data/boxrightend.pcx data/charbg.pcx data/health.pcx
pcx2clx --output-dir "${OUTPUT_DIR}/data" --num-sprites 2 data/talkbutton.pcx
pcx2clx --output-dir "${OUTPUT_DIR}/gendata" gendata/*.pcx
pcx2clx --output-dir "${OUTPUT_DIR}/ui_art" ui_art/creditsw.pcx ui_art/dvl_lrpopup.pcx ui_art/hf_titlew.pcx ui_art/mainmenuw.pcx ui_art/supportw.pcx

FONT_CONVERT_ARGS=(--transparent-color 1 --num-sprites 256)
for path in fonts/*.pcx; do
	if [[ -f "${path%.pcx}.txt" ]]; then
		pcx2clx "${FONT_CONVERT_ARGS[@]}" --output-dir "${OUTPUT_DIR}/fonts" --crop-widths "$(cat "${path%.pcx}.txt" | paste -sd , -)" "${path}"
	else
		pcx2clx "${FONT_CONVERT_ARGS[@]}" --output-dir "${OUTPUT_DIR}/fonts" "${path}"
	fi
done
for lang_dir in fonts/*/; do
	lang_dir="${lang_dir%/}" # remove trailing slash
	mkdir -p "${OUTPUT_DIR}/${lang_dir}"
	for path in "$lang_dir"/*.pcx; do
		if [[ -f "${path%.pcx}.txt" ]]; then
			pcx2clx "${FONT_CONVERT_ARGS[@]}" --output-dir "${OUTPUT_DIR}/${lang_dir}" --crop-widths "$(cat "${path%.pcx}.txt" | paste -sd , -)" "${path}"
		else
			pcx2clx "${FONT_CONVERT_ARGS[@]}" --output-dir "${OUTPUT_DIR}/${lang_dir}" "${path}"
		fi
	done
done

pcx2clx --num-sprites 2 --output-dir "${OUTPUT_DIR}/ui_art" ui_art/dvl_but_sml.pcx
pcx2clx --transparent-color 1 --output-dir "${OUTPUT_DIR}/data" data/hintbox.pcx data/hintboxbackground.pcx
pcx2clx --transparent-color 1 --num-sprites 6 --output-dir "${OUTPUT_DIR}/data" data/hinticons.pcx
pcx2clx --num-sprites 2 --output-dir "${OUTPUT_DIR}/data" data/panel8buc.pcx data/dirtybuc.pcx data/dirtybucp.pcx
pcx2clx --transparent-color 1 --output-dir "${OUTPUT_DIR}/data" data/healthbox.pcx
pcx2clx --transparent-color 1 --num-sprites 6 --output-dir "${OUTPUT_DIR}/data" data/resistance.pcx
pcx2clx --transparent-color 1 --num-sprites 5 --output-dir "${OUTPUT_DIR}/data" data/monstertags.pcx
pcx2clx --transparent-color 1 --output-dir "${OUTPUT_DIR}/data" data/stash.pcx
pcx2clx --transparent-color 1 --num-sprites 5 --output-dir "${OUTPUT_DIR}/data" data/stashnavbtns.pcx
pcx2clx --num-sprites 6 --output-dir "${OUTPUT_DIR}/data" data/panel8bucp.pcx
pcx2clx --transparent-color 1 --output-dir "${OUTPUT_DIR}/data" data/xpbar.pcx

# Shadow corrections are stored next to the sprite files that they belong to,
# those of Hellfire's sprites in the Hellfire mod.
COMPILE_SHADOW_FIXES="${OLDPWD}/tools/compile_shadow_fixes.py"
find . -path ./mods -prune -o -name '*.shadowfix.txt' -print0 | xargs -0 -r "${COMPILE_SHADOW_FIXES}" --output-dir "${OUTPUT_DIR}"
(cd mods/hf && find . -name '*.shadowfix.txt' -print0 | xargs -0 -r "${COMPILE_SHADOW_FIXES}" --output-dir "${OUTPUT_DIR}/../mods/hf")
cd -
