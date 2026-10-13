/**
 * preset_smoke — offline validation harness for the runtime slang preset
 * loader (GPU scaler "shaders folder" feature).
 *
 * Runs the full load pipeline minus the GPU device: scans a directory for
 * .slangp presets, parses them, translates every .slang pass and compiles it
 * to SPIR-V with the in-process glslang frontend. No SDL_GPU device is
 * created, so this runs headless (CI / sandbox friendly).
 *
 * Exit code 0 when every preset compiles (or passes --allow-failures with
 * at least one success). Prints per-preset diagnostics.
 *
 * Usage: preset_smoke [--root <dir>] [--limit N] [--quiet]
 */

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_set>
#include <vector>

#include "engine/render/gpu/preset/preset_parser.hpp"

namespace preset = devilution::gpu_scaler::preset;
#include "engine/render/gpu/preset/slang_compile.hpp"
#include "engine/render/gpu/preset/slang_translate.hpp"

#include "SPIRV/disassemble.h"
#include <sstream>

int main(int argc, char **argv)
{
	std::string root;
	std::string dumpDir;
	bool disasm = false;
	int limit = -1;
	bool quiet = false;
	for (int i = 1; i < argc; ++i) {
		const std::string arg = argv[i];
		if (arg == "--root" && i + 1 < argc) {
			root = argv[++i];
		} else if (arg == "--limit" && i + 1 < argc) {
			limit = std::atoi(argv[++i]);
		} else if (arg == "--quiet") {
			quiet = true;
		} else if (arg == "--dump" && i + 1 < argc) {
			dumpDir = argv[++i];
		} else if (arg == "--disasm") {
			disasm = true;
		}
	}
	if (root.empty()) {
		std::fprintf(stderr, "usage: preset_smoke --root <dir> [--limit N] [--quiet]\n");
		return 2;
	}

	const std::vector<preset::PresetEntry> entries = preset::ScanPresets(root);
	std::printf("scanned %zu presets in %s\n", entries.size(), root.c_str());
	if (entries.empty())
		return 2;

	int okCount = 0;
	int failCount = 0;
	int passTotal = 0;
	std::string firstFailure;
	std::string firstFailurePreset;

	const int runLimit = limit < 0 ? static_cast<int>(entries.size()) : std::min<int>(limit, static_cast<int>(entries.size()));
	for (int i = 0; i < runLimit; ++i) {
		const preset::PresetEntry &entry = entries[static_cast<size_t>(i)];
		std::filesystem::path presetPath = std::filesystem::path(root) / entry.relativePath;
		const std::string path = presetPath.string();
		preset::Preset preset;
		std::string error;
		if (!preset::ParsePreset(path, preset, error)) {
			++failCount;
			if (firstFailure.empty()) {
				firstFailure = "parse: " + error;
				firstFailurePreset = entry.displayName;
			}
			if (!quiet)
				std::printf("FAIL %s: %s\n", entry.displayName.c_str(), error.c_str());
			continue;
		}

		bool presetOk = true;
		std::string presetError;
		for (size_t p = 0; p < preset.passes.size(); ++p) {
			const std::string slangPath = ResolvePresetFilePath(preset.dir, preset.passes[p].shaderPath);
			// Build the same known-name set the runner uses: lookup texture
			// names declared by the preset plus every pass alias.
			std::unordered_set<std::string> known;
			auto insertBoth = [&known](const std::string &n) {
				if (n.empty())
					return;
				known.insert(n);
				std::string lower;
				lower.reserve(n.size());
				for (const char c : n)
					lower += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
				known.insert(lower);
			};
			for (const preset::TextureDef &tex : preset.textures)
				insertBoth(tex.name);
			for (const preset::PassDef &pass : preset.passes)
				insertBoth(pass.alias);
			preset::SlangTranslation translation;
			std::vector<preset::ParamInfo> presetParams;
			for (const auto &override_ : preset.parameters) {
				preset::ParamInfo info;
				info.name = override_.name;
				info.defaultValue = override_.value;
				presetParams.push_back(info);
			}
			if (!preset::TranslateSlang(slangPath, known, static_cast<int>(p),
			        static_cast<int>(preset.passes.size()), translation, presetParams)) {
				presetOk = false;
				presetError = "translate pass " + std::to_string(p) + " (" + preset.passes[p].shaderPath
				    + "): " + translation.error;
				break;
			}
			preset::PassSpirv spirv;
			// No disk cache for the smoke run: always compile fresh.
			if (!preset::CompilePass(translation, slangPath, "", spirv, error)) {
				presetOk = false;
				presetError = "compile pass " + std::to_string(p) + " (" + preset.passes[p].shaderPath
				    + "): " + error;
				break;
			}
			if (spirv.vertex.empty() || spirv.fragment.empty()) {
				presetOk = false;
				presetError = "empty SPIR-V in pass " + std::to_string(p);
				break;
			}
			if (!dumpDir.empty()) {
				std::filesystem::create_directories(dumpDir);
				const std::string base = dumpDir + "/" + entry.displayName + ".p" + std::to_string(p);
				auto writeBytes = [](const std::string &path, const void *data, size_t size) {
					std::ofstream f(path, std::ios::binary);
					f.write(static_cast<const char *>(data), static_cast<std::streamsize>(size));
				};
				writeBytes(base + ".vert.glsl", translation.vertex.glsl.data(), translation.vertex.glsl.size());
				writeBytes(base + ".frag.glsl", translation.fragment.glsl.data(), translation.fragment.glsl.size());
				writeBytes(base + ".vert.spv", spirv.vertex.data(), spirv.vertex.size());
				writeBytes(base + ".frag.spv", spirv.fragment.data(), spirv.fragment.size());
				if (disasm) {
					auto dis = [](const std::vector<uint8_t> &blob) {
						std::vector<unsigned int> words(blob.size() / sizeof(unsigned int));
						std::memcpy(words.data(), blob.data(), words.size() * sizeof(unsigned int));
						std::ostringstream os;
						spv::Disassemble(os, words);
						return os.str();
					};
					std::ofstream fv(base + ".vert.spvasm");
					fv << dis(spirv.vertex);
					std::ofstream ff(base + ".frag.spvasm");
					ff << dis(spirv.fragment);
				}
				std::ofstream summary(base + ".summary.txt");
				auto dumpStage = [&summary](const char *name, const preset::StageTranslation &st, bool isVertex) {
					const uint32_t uboSet = isVertex ? 1 : 3;
					const uint32_t texSet = isVertex ? 0 : 2;
					summary << "[" << name << "] present=" << st.present << "\n";
					for (const preset::UniformBlock &b : st.uniformBlocks)
						summary << "  ubo instance='" << b.instanceName << "' set=" << uboSet
						        << " binding=" << b.binding << " members=" << b.members.size() << "\n";
					for (const preset::TextureRef &t : st.textures)
						summary << "  texture '" << t.name << "' set=" << texSet << " binding=" << t.binding << "\n";
				};
				dumpStage("vertex", translation.vertex, true);
				dumpStage("fragment", translation.fragment, false);
			}
			++passTotal;
		}
		if (presetOk) {
			++okCount;
			if (!quiet)
				std::printf("ok   %s (%zu passes)\n", entry.displayName.c_str(), preset.passes.size());
		} else {
			++failCount;
			if (firstFailure.empty()) {
				firstFailure = presetError;
				firstFailurePreset = entry.displayName;
			}
			if (!quiet)
				std::printf("FAIL %s: %s\n", entry.displayName.c_str(), presetError.c_str());
		}
	}

	std::printf("\nresult: %d ok, %d failed, %d passes compiled\n", okCount, failCount, passTotal);
	if (!firstFailure.empty())
		std::printf("first failure (%s): %s\n", firstFailurePreset.c_str(), firstFailure.c_str());
	return failCount == 0 ? 0 : 1;
}
