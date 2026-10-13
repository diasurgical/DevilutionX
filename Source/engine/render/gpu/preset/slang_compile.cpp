#include "engine/render/gpu/preset/slang_compile.hpp"

#ifdef USE_GPU_SCALER
#ifndef USE_SDL1

#include <atomic>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>

#include <SPIRV/GlslangToSpv.h>
#include <glslang/Public/ResourceLimits.h>
#include <glslang/Public/ShaderLang.h>

#include "utils/log.hpp"
#include "utils/str_cat.hpp"

// SHA-256 for content-addressed cache keys (header-only, already vendored).
#include <picosha2.h>

namespace devilution {
namespace gpu_scaler {
namespace preset {
namespace {

/** @brief Process-wide glslang lifecycle guard. */
class GlslangRuntime {
public:
	static GlslangRuntime &Instance()
	{
		static GlslangRuntime runtime;
		return runtime;
	}

	GlslangRuntime()
	{
		glslang::InitializeProcess();
	}
	~GlslangRuntime()
	{
		glslang::FinalizeProcess();
	}
	GlslangRuntime(const GlslangRuntime &) = delete;
	GlslangRuntime &operator=(const GlslangRuntime &) = delete;
};

bool CompileStageToSpv(EShLanguage stage, const std::string &source, std::vector<uint8_t> &out, std::string &error)
{
	GlslangRuntime::Instance();

	glslang::TShader shader(stage);
	const char *sourcePtr = source.c_str();
	shader.setStrings(&sourcePtr, 1);
	shader.setEnvInput(glslang::EShSourceGlsl, stage, glslang::EShClientVulkan, 100);
	shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_0);
	shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_0);
	const EShMessages messages = static_cast<EShMessages>(EShMsgSpvRules | EShMsgVulkanRules);
	if (!shader.parse(GetDefaultResources(), 450, false, messages)) {
		error = StrCat("glslang parse failed (", stage == EShLangVertex ? "vertex" : "fragment", "): ",
		    shader.getInfoLog());
		return false;
	}
	glslang::TProgram program;
	program.addShader(&shader);
	if (!program.link(EShMsgDefault)) {
		error = StrCat("glslang link failed: ", program.getInfoLog());
		return false;
	}
	std::vector<unsigned int> spirv;
	glslang::GlslangToSpv(*program.getIntermediate(stage), spirv);
	out.resize(spirv.size() * sizeof(uint32_t));
	std::memcpy(out.data(), spirv.data(), out.size());
	return true;
}

bool ReadFileBytes(const std::string &path, std::vector<uint8_t> &out)
{
	std::ifstream stream(path, std::ios::binary);
	if (!stream)
		return false;
	std::ostringstream ss;
	ss << stream.rdbuf();
	const std::string str = ss.str();
	out.assign(str.begin(), str.end());
	return true;
}

bool WriteFileBytes(const std::string &path, const std::vector<uint8_t> &data)
{
	std::error_code ec;
	const std::filesystem::path parent = std::filesystem::path(path).parent_path();
	std::filesystem::create_directories(parent, ec);
	std::ofstream stream(path, std::ios::binary | std::ios::trunc);
	if (!stream)
		return false;
	stream.write(reinterpret_cast<const char *>(data.data()), static_cast<std::streamsize>(data.size()));
	return stream.good();
}

bool SpvLooksValid(const std::vector<uint8_t> &data)
{
	// SPIR-V magic number 0x07230203, little-endian.
	static constexpr uint8_t kMagic[4] = { 0x03, 0x02, 0x23, 0x07 };
	return data.size() >= 20 && data.size() % 4 == 0 && std::memcmp(data.data(), kMagic, 4) == 0;
}

} // namespace

void CachePathFor(const SlangTranslation &translation, const std::string &slangPath, const std::string &cacheDir,
    std::string &vertexPath, std::string &fragmentPath)
{
	vertexPath.clear();
	fragmentPath.clear();
	if (cacheDir.empty())
		return;
	// Content-addressed key: any change to the .slang source, its includes or
	// the translator's output changes the hash, so stale SPIR-V is never
	// served. The source path is mixed in only for readable cache names.
	std::string hash;
	picosha2::hash256_hex_string(translation.vertex.glsl + std::string("\x1f", 1) + translation.fragment.glsl, hash);
	const std::string stem = std::filesystem::path(slangPath).stem().string();
	const std::string base = StrCat(cacheDir, "/slang-", stem.empty() ? "pass" : stem, "-", hash.substr(0, 24));
	vertexPath = base + ".vert.spv";
	fragmentPath = base + ".frag.spv";
}

void SidecarSpvPathsFor(const std::string &slangPath, std::string &vertexPath, std::string &fragmentPath)
{
	std::filesystem::path p(slangPath);
	vertexPath = (p.parent_path() / (p.stem().string() + ".vert.spv")).string();
	fragmentPath = (p.parent_path() / (p.stem().string() + ".frag.spv")).string();
}

bool CompilePass(const SlangTranslation &translation, const std::string &slangPath, const std::string &cacheDir,
    PassSpirv &out, std::string &error)
{
	out.vertex.clear();
	out.fragment.clear();

	if (!translation.vertex.present || !translation.fragment.present) {
		error = "slang: pass is missing a required stage";
		return false;
	}

	// 1. Precompiled sidecar files next to the source always win.
	std::string sidecarVert, sidecarFrag;
	SidecarSpvPathsFor(slangPath, sidecarVert, sidecarFrag);
	if (ReadFileBytes(sidecarVert, out.vertex) && SpvLooksValid(out.vertex) && ReadFileBytes(sidecarFrag, out.fragment)
	    && SpvLooksValid(out.fragment)) {
		Log("GPU scaler preset: using precompiled SPIR-V sidecars for {}", slangPath);
		return true;
	}

	// 2. Disk cache keyed by a hash of the translated sources.
	std::string cacheVert, cacheFrag;
	CachePathFor(translation, slangPath, cacheDir, cacheVert, cacheFrag);
	if (!cacheVert.empty() && ReadFileBytes(cacheVert, out.vertex) && SpvLooksValid(out.vertex)
	    && ReadFileBytes(cacheFrag, out.fragment) && SpvLooksValid(out.fragment)) {
		Log("GPU scaler preset: cache hit for {}", slangPath);
		return true;
	}

	// 3. Compile with glslang.
	if (!CompileStageToSpv(EShLangVertex, translation.vertex.glsl, out.vertex, error))
		return false;
	if (!CompileStageToSpv(EShLangFragment, translation.fragment.glsl, out.fragment, error))
		return false;

	// 4. Store in the cache (best-effort).
	if (!cacheVert.empty()) {
		if (!WriteFileBytes(cacheVert, out.vertex) || !WriteFileBytes(cacheFrag, out.fragment))
			Log("GPU scaler preset: could not write SPIR-V cache files for {}", slangPath);
	}
	return true;
}

} // namespace preset
} // namespace gpu_scaler
} // namespace devilution
#endif // !USE_SDL1
#endif // USE_GPU_SCALER
