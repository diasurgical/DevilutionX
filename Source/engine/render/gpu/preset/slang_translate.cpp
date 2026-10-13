#include "engine/render/gpu/preset/slang_translate.hpp"

#ifdef USE_GPU_SCALER
#ifndef USE_SDL1

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <regex>
#include <set>
#include <sstream>
#include <unordered_map>

#include "utils/str_cat.hpp"

namespace devilution {
namespace gpu_scaler {
namespace preset {
namespace {

std::string TrimAll(const std::string &s)
{
	size_t begin = 0;
	size_t end = s.size();
	while (begin < end && std::isspace(static_cast<unsigned char>(s[begin])) != 0)
		++begin;
	while (end > begin && std::isspace(static_cast<unsigned char>(s[end - 1])) != 0)
		--end;
	return s.substr(begin, end - begin);
}

/** @brief Strips line and block comments from a copy of the text (for structural parsing). */
std::string StripComments(const std::string &text)
{
	std::string out;
	out.reserve(text.size());
	bool inLineComment = false;
	bool inBlockComment = false;
	for (size_t i = 0; i < text.size(); ++i) {
		const char c = text[i];
		const char next = i + 1 < text.size() ? text[i + 1] : '\0';
		if (inLineComment) {
			if (c == '\n') {
				inLineComment = false;
				out += c;
			}
			continue;
		}
		if (inBlockComment) {
			if (c == '*' && next == '/') {
				inBlockComment = false;
				++i;
			} else if (c == '\n') {
				out += c; // Keep line structure.
			}
			continue;
		}
		if (c == '/' && next == '/') {
			inLineComment = true;
			++i;
			continue;
		}
		if (c == '/' && next == '*') {
			inBlockComment = true;
			++i;
			continue;
		}
		out += c;
	}
	return out;
}

/** @brief Resolves #include directives recursively, relative to the including file. */
bool ResolveIncludes(const std::string &path, int depth, std::set<std::string> &visited,
    std::string &out, std::string &error)
{
	constexpr int kMaxDepth = 24;
	if (depth > kMaxDepth) {
		error = StrCat("slang: include depth limit exceeded at ", path);
		return false;
	}
	std::error_code ec;
	const std::string canonical = std::filesystem::weakly_canonical(path, ec).string();
	if (!visited.insert(canonical.empty() ? path : canonical).second) {
		// Already included once; drop repeats instead of looping (the spec
		// leaves include cycles undefined).
		return true;
	}
	std::ifstream stream(path, std::ios::binary);
	if (!stream) {
		error = StrCat("slang: cannot open shader file: ", path);
		return false;
	}
	std::ostringstream ss;
	ss << stream.rdbuf();
	std::string content = ss.str();
	content.erase(std::remove(content.begin(), content.end(), '\r'), content.end());

	std::string dir = std::filesystem::path(path).parent_path().string();
	if (!dir.empty() && dir.back() != '/' && dir.back() != '\\')
		dir += '/';

	static const std::regex includeRe(R"re(^\s*#include\s+"([^"]+)"\s*$)re");
	std::istringstream lines(content);
	std::string line;
	while (std::getline(lines, line)) {
		if (line.rfind("#version", 0) == 0)
			continue; // Only the top-level file may carry #version.
		std::smatch match;
		if (std::regex_match(line, match, includeRe)) {
			const std::string incPath = match[1].str();
			// Spec: relative to the including file. Fall back to the primary
			// shader's directory for presets that assume a flat layout.
			std::filesystem::path candidate = std::filesystem::path(dir) / incPath;
			if (!std::filesystem::exists(candidate, ec))
				candidate = std::filesystem::path(path).parent_path() / incPath;
			out += "// begin include: " + incPath + "\n";
			if (!ResolveIncludes(candidate.string(), depth + 1, visited, out, error))
				return false;
			out += "// end include: " + incPath + "\n";
		} else {
			out += line;
			out += '\n';
		}
	}
	return true;
}

struct StageSplit {
	std::string shared;
	std::string vertex;
	std::string fragment;
};

StageSplit SplitStages(const std::string &text)
{
	StageSplit split;
	static const std::regex stageRe(R"(^\s*#pragma\s+stage\s+(vertex|fragment)\s*$)");
	std::istringstream lines(text);
	std::string line;
	int stage = 0; // 0 = shared, 1 = vertex, 2 = fragment
	while (std::getline(lines, line)) {
		std::smatch match;
		if (std::regex_match(line, match, stageRe)) {
			stage = (match[1] == "vertex") ? 1 : 2;
			continue;
		}
		switch (stage) {
		case 0:
			split.shared += line;
			split.shared += '\n';
			break;
		case 1:
			split.vertex += line;
			split.vertex += '\n';
			break;
		case 2:
			split.fragment += line;
			split.fragment += '\n';
			break;
		}
	}
	return split;
}

/** @brief Extracts and removes `#pragma name/format/parameter` lines. */
void ExtractPragmas(std::string &text, std::vector<ParamInfo> &parameters, std::string &pragmaName,
    std::string &pragmaFormat)
{
	static const std::regex paramRe(
	    R"re(^\s*#pragma\s+parameter\s+(\w+)\s+"([^"]*)"\s+([-+0-9.eE]+)\s+([-+0-9.eE]+)\s+([-+0-9.eE]+)(?:\s+([-+0-9.eE]+))?\s*$)re");
	static const std::regex nameRe(R"(^\s*#pragma\s+name\s+(\w+)\s*$)");
	static const std::regex formatRe(R"(^\s*#pragma\s+format\s+(\w+)\s*$)");

	std::istringstream lines(text);
	std::string line;
	std::string kept;
	while (std::getline(lines, line)) {
		std::smatch match;
		if (std::regex_match(line, match, paramRe)) {
			ParamInfo param;
			param.name = match[1].str();
			param.description = match[2].str();
			param.defaultValue = std::stof(match[3].str());
			param.minimum = std::stof(match[4].str());
			param.maximum = std::stof(match[5].str());
			if (match[6].matched)
				param.step = std::stof(match[6].str());
			parameters.push_back(std::move(param));
			continue;
		}
		if (std::regex_match(line, match, nameRe)) {
			pragmaName = match[1].str();
			continue;
		}
		if (std::regex_match(line, match, formatRe)) {
			pragmaFormat = match[1].str();
			continue;
		}
		kept += line;
		kept += '\n';
	}
	text = std::move(kept);
}

struct BlockMatch {
	size_t matchStart;   // Offset of the layout prefix (or of "uniform" without one).
	size_t uniformBegin; // Offset of the "uniform" keyword.
	size_t bodyBegin;    // Offset just past '{'.
	size_t bodyEnd;      // Offset of the matching '}'.
	size_t declEnd;      // Offset just past the terminating ';' of the instance declaration.
	std::string blockName;
	std::string instanceName;
	std::string body;
};

/**
 * @brief Finds the next uniform block declaration that matches the requested kind.
 * @param pushConstant true to find the push-constant block, false for plain UBOs.
 */
bool FindUniformBlock(const std::string &text, size_t from, bool pushConstant, BlockMatch &out)
{
	static const std::regex headRe(R"((layout\s*\([^;{]*?\)\s*)?uniform\s+(\w+)\s*\{)");
	std::smatch match;
	auto searchFrom = text.cbegin() + static_cast<long>(from);
	while (std::regex_search(searchFrom, text.cend(), match, headRe)) {
		const bool isPush = match[1].matched && match[1].str().find("push_constant") != std::string::npos;
		if (isPush != pushConstant) {
			searchFrom = match.suffix().first;
			continue;
		}
		BlockMatch block;
		// match.position() is relative to the current search range start, which the
		// skip loop may have advanced past `from` (e.g. a push-constant block that
		// was skipped while looking for a plain UBO). Anchor to searchFrom instead.
		const size_t searchOffset = static_cast<size_t>(searchFrom - text.cbegin());
		block.matchStart = searchOffset + static_cast<size_t>(match.position(0));
		block.uniformBegin = block.matchStart + (match[1].matched ? match[1].length() : 0);
		block.bodyBegin = searchOffset + static_cast<size_t>(match.position(0)) + match.length();
		block.blockName = match[2].str();
		int depth = 1;
		size_t i = block.bodyBegin;
		for (; i < text.size() && depth > 0; ++i) {
			if (text[i] == '{')
				++depth;
			else if (text[i] == '}')
				--depth;
		}
		if (depth != 0)
			return false; // Unbalanced; glslang will report it.
		block.bodyEnd = i - 1;
		block.body = text.substr(block.bodyBegin, block.bodyEnd - block.bodyBegin);
		size_t j = block.bodyEnd + 1;
		while (j < text.size() && std::isspace(static_cast<unsigned char>(text[j])) != 0)
			++j;
		std::string instance;
		while (j < text.size() && (std::isalnum(static_cast<unsigned char>(text[j])) != 0 || text[j] == '_')) {
			instance += text[j];
			++j;
		}
		while (j < text.size() && std::isspace(static_cast<unsigned char>(text[j])) != 0)
			++j;
		if (j < text.size() && text[j] == ';')
			++j;
		block.instanceName = instance;
		block.declEnd = j;
		out = std::move(block);
		return true;
	}
	return false;
}

bool ParseUniformType(const std::string &typeToken, UniformType &type)
{
	static const std::unordered_map<std::string, UniformType> kTypes = {
		{ "float", UniformType::Float },
		{ "vec2", UniformType::Vec2 },
		{ "vec3", UniformType::Vec3 },
		{ "vec4", UniformType::Vec4 },
		{ "int", UniformType::Int },
		{ "ivec2", UniformType::IVec2 },
		{ "ivec3", UniformType::IVec3 },
		{ "ivec4", UniformType::IVec4 },
		{ "uint", UniformType::Uint },
		{ "uvec2", UniformType::UVec2 },
		{ "uvec3", UniformType::UVec3 },
		{ "uvec4", UniformType::UVec4 },
		{ "bool", UniformType::Bool },
		{ "mat4", UniformType::Mat4 },
		{ "mat4x4", UniformType::Mat4 },
	};
	const auto it = kTypes.find(typeToken);
	if (it == kTypes.end())
		return false;
	type = it->second;
	return true;
}

void Std140TypeLayout(UniformType type, uint32_t &align, uint32_t &size)
{
	switch (type) {
	case UniformType::Float:
	case UniformType::Int:
	case UniformType::Uint:
	case UniformType::Bool:
		align = 4;
		size = 4;
		break;
	case UniformType::Vec2:
	case UniformType::IVec2:
	case UniformType::UVec2:
		align = 8;
		size = 8;
		break;
	case UniformType::Vec3:
	case UniformType::IVec3:
	case UniformType::UVec3:
		align = 16;
		size = 12;
		break;
	case UniformType::Vec4:
	case UniformType::IVec4:
	case UniformType::UVec4:
		align = 16;
		size = 16;
		break;
	case UniformType::Mat4:
		align = 16;
		size = 64;
		break;
	}
}

uint32_t AlignUp(uint32_t value, uint32_t alignment)
{
	return (value + alignment - 1) / alignment * alignment;
}

uint32_t BlockNextOffset(const std::vector<UniformMember> &members)
{
	if (members.empty())
		return 0;
	const UniformMember &last = members.back();
	return last.offset + last.size;
}

bool ParseBlockMembers(const std::string &body, std::vector<UniformMember> &members, std::string &error)
{
	const std::string clean = StripComments(body);
	static const std::regex layoutRe(R"(layout\s*\([^)]*\))");
	size_t pos = 0;
	while (pos < clean.size()) {
		const size_t semi = clean.find(';', pos);
		if (semi == std::string::npos)
			break;
		std::string decl = clean.substr(pos, semi - pos);
		pos = semi + 1;
		decl = std::regex_replace(decl, layoutRe, " ");
		for (const char *qualifier : { "flat", "smooth", "noperspective", "highp", "mediump", "lowp" }) {
			const size_t q = decl.find(qualifier);
			if (q != std::string::npos
			    && (q == 0 || std::isspace(static_cast<unsigned char>(decl[q - 1])) != 0)) {
				decl.erase(q, std::strlen(qualifier));
			}
		}
		std::istringstream words(decl);
		std::vector<std::string> tokens;
		std::string token;
		while (words >> token)
			tokens.push_back(token);
		if (tokens.empty())
			continue;
		if (tokens.size() < 2) {
			error = StrCat("slang: unsupported uniform block member: '", decl, "'");
			return false;
		}
		// "<type> <name>[N], <name2>" - the type is the first token naming a
		// supported std140 type; every token after it is a member name (this
		// also handles multi-name declarations like "float A, B;").
		size_t typeIdx = tokens.size();
		for (size_t t = 0; t + 1 < tokens.size(); ++t) {
			UniformType probe;
			if (ParseUniformType(tokens[t], probe)) {
				typeIdx = t;
				break;
			}
		}
		if (typeIdx >= tokens.size() - 1) {
			error = StrCat("slang: unsupported uniform block member: '", decl, "'");
			return false;
		}
		UniformType type;
		(void)ParseUniformType(tokens[typeIdx], type);
		uint32_t align = 0;
		uint32_t size = 0;
		Std140TypeLayout(type, align, size);
		for (size_t n = typeIdx + 1; n < tokens.size(); ++n) {
			std::string nameToken = tokens[n];
			size_t comma = 0;
			while ((comma = nameToken.find(',', comma)) != std::string::npos)
				nameToken.erase(comma, 1);
			if (nameToken.empty())
				continue;
			UniformMember member;
			member.type = type;
			const size_t bracket = nameToken.find('[');
			if (bracket != std::string::npos) {
				const size_t close = nameToken.find(']', bracket);
				const std::string countText = bracket + 1 <= close ? nameToken.substr(bracket + 1, close - bracket - 1) : "";
				member.name = nameToken.substr(0, bracket);
				if (countText.empty()) {
					error = StrCat("slang: unsized arrays are not supported in uniform blocks: '", member.name, "'");
					return false;
				}
				member.arraySize = std::stoi(countText);
				if (member.arraySize <= 0 || member.arraySize > 64) {
					error = StrCat("slang: invalid array size for member: '", member.name, "'");
					return false;
				}
			} else {
				member.name = nameToken;
			}
			if (member.arraySize > 0) {
				// std140: array stride is rounded up to a full vec4 (16 bytes).
				const uint32_t stride = AlignUp(size, std::max<uint32_t>(align, 16));
				member.size = stride * static_cast<uint32_t>(member.arraySize);
				member.offset = AlignUp(BlockNextOffset(members), std::max<uint32_t>(align, 16));
			} else {
				member.size = size;
				member.offset = AlignUp(BlockNextOffset(members), align);
			}
			members.push_back(std::move(member));
		}
	}
	return true;
}

/** @brief Rewrites plain UBO declarations to the SDL descriptor set conventions. */
bool RewriteUboBlocks(std::string &glsl, bool isVertex, std::vector<UniformBlock> &blocks, std::string &error)
{
	const uint32_t set = isVertex ? 1 : 3;
	uint32_t binding = 0;
	size_t from = 0;
	for (;;) {
		BlockMatch block;
		if (!FindUniformBlock(glsl, from, /*pushConstant=*/false, block))
			break;
		if (blocks.size() >= 1) {
			error = "slang: more than one uniform block is not supported (spec allows one UBO)";
			return false;
		}
		if (binding >= 2) {
			error = "slang: too many uniform blocks";
			return false;
		}
		UniformBlock ubo;
		ubo.binding = binding;
		ubo.instanceName = block.instanceName;
		if (!ParseBlockMembers(block.body, ubo.members, error))
			return false;
		const std::string replacement = StrCat("layout(std140, set = ", set, ", binding = ", binding, ")");
		glsl = glsl.substr(0, block.matchStart) + replacement + glsl.substr(block.uniformBegin);
		blocks.push_back(std::move(ubo));
		// Continue searching after the rewritten declaration. The replacement has
		// consumed the original layout qualifier, so the next position is
		// matchStart + |replacement| + (declEnd - uniformBegin).
		from = block.matchStart + replacement.size() + (block.declEnd - block.uniformBegin);
		++binding;
	}
	return true;
}

/** @brief Re-homes the push-constant block as a std140 UBO in the SDL conventions. */
bool RewritePushBlock(std::string &glsl, bool isVertex, bool uboPresent, std::vector<UniformBlock> &blocks,
    std::string &error)
{
	const uint32_t set = isVertex ? 1 : 3;
	const uint32_t binding = uboPresent ? 1 : 0;
	BlockMatch block;
	if (!FindUniformBlock(glsl, 0, /*pushConstant=*/true, block))
		return true; // No push block: fine.
	UniformBlock pushBlock;
	pushBlock.binding = binding;
	pushBlock.instanceName = block.instanceName;
	if (!ParseBlockMembers(block.body, pushBlock.members, error))
		return false;
	const std::string replacement = StrCat("layout(std140, set = ", set, ", binding = ", binding, ")");
	glsl = glsl.substr(0, block.matchStart) + replacement + glsl.substr(block.uniformBegin);
	blocks.push_back(std::move(pushBlock));
	return true;
}

bool IsBuiltinTextureName(const std::string &name)
{
	static const std::regex builtinRe(R"(^(Source|Original|(OriginalHistory|PassOutput|PassFeedback|User)(0|[1-9][0-9]*))$)");
	return std::regex_match(name, builtinRe);
}

int BuiltinTextureIndex(const std::string &name)
{
	static const std::regex indexedRe(R"(^(OriginalHistory|PassOutput|PassFeedback|User)(0|[1-9][0-9]*)$)");
	std::smatch match;
	if (!std::regex_match(name, match, indexedRe))
		return -1;
	return std::stoi(match[2].str());
}

/** @brief Rewrites sampler2D declarations to the SDL3 resource set (0 vertex, 2 fragment) with dense bindings. */
bool RewriteSamplers(std::string &glsl, bool isVertex, StageTranslation &stage,
    const std::unordered_set<std::string> &knownTextureNames, int passIndex, int passCount, std::string &error)
{
	static const std::regex samplerRe(
	    R"(^(\s*)(layout\s*\([^)]*\)\s*)?uniform\s+((?:lowp\s+|mediump\s+|highp\s+)?)sampler2D\s+(.+?)\s*;\s*$)");
	std::istringstream lines(glsl);
	std::string line;
	std::string out;
	out.reserve(glsl.size() + 64);
	const uint32_t texSet = isVertex ? 0U : 2U;
	uint32_t binding = 0;
	while (std::getline(lines, line)) {
		std::smatch match;
		if (!std::regex_match(line, match, samplerRe)) {
			out += line;
			out += '\n';
			continue;
		}
		const std::string precision = match[3].str();
		std::string namesText = match[4].str();
		// Split on commas: "A, B" (arrays are not supported).
		std::vector<std::string> names;
		std::istringstream nameStream(namesText);
		std::string nameToken;
		while (std::getline(nameStream, nameToken, ',')) {
			names.push_back(TrimAll(nameToken));
		}
		for (const std::string &name : names) {
			if (name.empty()) {
				error = "slang: empty sampler name";
				return false;
			}
			if (name.find('[') != std::string::npos) {
				error = StrCat("slang: sampler arrays are not supported: '", name, "'");
				return false;
			}
			if (!IsBuiltinTextureName(name)) {
				std::string base = name;
				if (base.size() > 8 && base.compare(base.size() - 8, 8, "Feedback") == 0)
					base = base.substr(0, base.size() - 8);
				// Case-insensitive match against the preset's declared names:
				// GLSL is case-sensitive and presets/shaders disagree on case.
				std::string baseLower;
				baseLower.reserve(base.size());
				for (const char c : base)
					baseLower += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
				if (knownTextureNames.find(baseLower) == knownTextureNames.end()) {
					error = StrCat("slang: unknown texture reference '", name, "' (not a builtin, alias or lookup texture)");
					return false;
				}
			}
			// PassOutput# causality: pass N may only read outputs of passes < N.
			if (name.rfind("PassOutput", 0) == 0) {
				const int idx = BuiltinTextureIndex(name);
				if (idx >= passIndex || idx >= passCount) {
					error = StrCat("slang: PassOutput", idx, " is not causal in pass ", passIndex);
					return false;
				}
			}
			if (name.rfind("PassFeedback", 0) == 0) {
				const int idx = BuiltinTextureIndex(name);
				if (idx >= passCount || idx < 0) {
					error = StrCat("slang: PassFeedback", idx, " does not exist (", passCount, " passes)");
					return false;
				}
			}
			if (name.rfind("OriginalHistory", 0) == 0) {
				const int idx = BuiltinTextureIndex(name);
				if (idx < 0 || idx > 8) {
					error = StrCat("slang: OriginalHistory", idx, " is out of range (0-8)");
					return false;
				}
			}
			out += StrCat("layout(set = ", texSet, ", binding = ", binding, ") uniform ", precision, "sampler2D ", name, ";\n");
			stage.textures.push_back({ name, binding });
			++binding;
		}
	}
	glsl = std::move(out);
	if (isVertex && !stage.textures.empty()) {
		error = "slang: vertex-stage texture sampling is not supported yet";
		return false;
	}
	return true;
}

/** @brief Validates that every uniform block member is meaningful (spec requirement). */
bool ValidateBlockMembers(const std::vector<UniformBlock> &blocks,
    const std::vector<ParamInfo> &parameters, const std::unordered_set<std::string> &knownTextureNames,
    std::string &error)
{
	for (const UniformBlock &block : blocks) {
		for (const UniformMember &member : block.members) {
			const std::string &name = member.name;
			bool ok = (name == "MVP" || name == "OutputSize" || name == "FinalViewportSize" || name == "OriginalSize"
			    || name == "SourceSize" || name == "FrameCount" || name == "FrameDirection" || name == "FrameTime"
			    || name == "PASSINDEX");
			if (!ok) {
				std::string base = name;
				bool sizeForm = false;
				bool feedbackSizeForm = false;
				if (base.size() > 4 && base.compare(base.size() - 4, 4, "Size") == 0) {
					base = base.substr(0, base.size() - 4);
					sizeForm = true;
				}
				if (base.size() > 8 && base.compare(base.size() - 8, 8, "Feedback") == 0) {
					base = base.substr(0, base.size() - 8);
					feedbackSizeForm = true;
				}
				if ((sizeForm || feedbackSizeForm)
				    && (base == "Original" || base == "Source" || knownTextureNames.find(base) != knownTextureNames.end()
				        || IsBuiltinTextureName(base))) {
					ok = true;
				}
			}
			if (!ok) {
				for (const ParamInfo &param : parameters) {
					if (param.name.size() == name.size()
					    && std::equal(param.name.begin(), param.name.end(), name.begin(),
					        [](char a, char b) { return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b)); })) {
						ok = true;
						break;
					}
				}
			}
			if (!ok) {
				error = StrCat("slang: unknown uniform block member '", name,
				    "' (not a builtin semantic, texture size, feedback size or #pragma parameter)");
				return false;
			}
		}
	}
	return true;
}

/** @brief Ensures the vertex stage declares the two required inputs and a #version. */
void EnsureVertexInterface(std::string &glsl)
{
	const bool hasVersion = glsl.rfind("#version", 0) == 0;
	static const std::regex posInRe(R"(layout\s*\(\s*location\s*=\s*0\s*\)\s*in\s+vec4\s+\w+)");
	static const std::regex texInRe(R"(layout\s*\(\s*location\s*=\s*1\s*\)\s*in\s+vec2\s+\w+)");
	const bool hasPosition = std::regex_search(glsl, posInRe);
	const bool hasTexCoord = std::regex_search(glsl, texInRe);
	std::string prologue;
	if (!hasVersion)
		prologue += "#version 450\n";
	if (!hasPosition)
		prologue += "layout(location = 0) in vec4 Position;\n";
	if (!hasTexCoord)
		prologue += "layout(location = 1) in vec2 TexCoord;\n";
	if (!prologue.empty()) {
		if (hasVersion) {
			// Insert after the #version line.
			const size_t eol = glsl.find('\n');
			glsl = glsl.substr(0, eol + 1) + prologue + glsl.substr(eol + 1);
		} else {
			glsl = prologue + glsl;
		}
	}
}

/** @brief Injects the stock vertex stage when a shader only provides a fragment stage. */
std::string StockVertexSource()
{
	return R"(#version 450
layout(std140, set = 1, binding = 0) uniform UBO
{
    mat4 MVP;
    vec4 OutputSize;
    vec4 OriginalSize;
    vec4 SourceSize;
} global;

layout(location = 0) in vec4 Position;
layout(location = 1) in vec2 TexCoord;
layout(location = 0) out vec2 vTexCoord;

void main()
{
    gl_Position = global.MVP * Position;
    vTexCoord = TexCoord;
}
)";
}

/** @brief FNV-1a 32-bit content fingerprint for remote field diagnostics. */
unsigned Fnv1a32(const std::string &data)
{
	unsigned hash = 2166136261u;
	for (const char c : data) {
		hash ^= static_cast<unsigned char>(c);
		hash *= 16777619u;
	}
	return hash;
}

/** @brief Whole-file bytes for fingerprinting (binary-safe). */
bool ReadWholeFileBytes(const std::string &path, std::string &out)
{
	std::ifstream in(path, std::ios::binary);
	if (!in)
		return false;
	std::ostringstream buffer;
	buffer << in.rdbuf();
	out = buffer.str();
	return true;
}

/** @brief Non-overlapping substring count. */
int CountOccurrences(const std::string &text, const char *needle)
{
	int count = 0;
	const size_t len = std::strlen(needle);
	for (size_t pos = text.find(needle); pos != std::string::npos; pos = text.find(needle, pos + len))
		++count;
	return count;
}

bool ProcessStage(std::string glsl, bool isVertex, int passIndex, int passCount,
    const std::unordered_set<std::string> &knownTextureNames, const std::vector<ParamInfo> &parameters,
    StageTranslation &stage, std::string &error)
{
	if (TrimAll(glsl).empty()) {
		stage.present = false;
		return true; // Stage absent in this shader (e.g. fragment-only file).
	}
	if (!RewriteUboBlocks(glsl, isVertex, stage.uniformBlocks, error))
		return false;
	const bool uboPresent = !stage.uniformBlocks.empty();
	if (!RewritePushBlock(glsl, isVertex, uboPresent, stage.uniformBlocks, error))
		return false;
	if (!RewriteSamplers(glsl, isVertex, stage, knownTextureNames, passIndex, passCount, error))
		return false;
	if (!ValidateBlockMembers(stage.uniformBlocks, parameters, knownTextureNames, error))
		return false;
	for (const UniformBlock &block : stage.uniformBlocks) {
		for (const UniformMember &member : block.members) {
			stage.usesMvp |= member.name == "MVP";
			stage.usesFrameCount |= member.name == "FrameCount";
			stage.usesFrameDirection |= member.name == "FrameDirection";
			stage.usesFrameTime |= member.name == "FrameTime";
			stage.usesPassIndex |= member.name == "PASSINDEX";
		}
	}
	if (isVertex) {
		EnsureVertexInterface(glsl);
	} else if (glsl.find("void main") == std::string::npos) {
		error = "slang: fragment stage has no main()";
		return false;
	}
	stage.texelFetchCount = CountOccurrences(glsl, "texelFetch");
	stage.textureGatherCount = CountOccurrences(glsl, "textureGather");
	stage.glsl = std::move(glsl);
	stage.present = true;
	return true;
}

} // namespace

bool TranslateSlang(const std::string &shaderPath, const std::unordered_set<std::string> &knownTextureNames,
    int passIndex, int passCount, SlangTranslation &out, const std::vector<ParamInfo> &presetParameters)
{
	out = SlangTranslation {};

	std::string error;
	std::string merged;
	std::set<std::string> includeStack;
	if (!ResolveIncludes(shaderPath, 0, includeStack, merged, error)) {
		out.error = error;
		return false;
	}
	// Field diagnostics: fingerprint the raw file and the include-resolved
	// source so a remote log can be diffed against known shader-pack versions.
	{
		std::string fileBytes;
		if (ReadWholeFileBytes(shaderPath, fileBytes)) {
			out.sourceSize = static_cast<int>(fileBytes.size());
			out.sourceHash = Fnv1a32(fileBytes);
		}
		out.mergedSize = static_cast<int>(merged.size());
		out.mergedHash = Fnv1a32(merged);
	}

	std::vector<ParamInfo> parameters;
	std::string pragmaName;
	std::string pragmaFormat;
	ExtractPragmas(merged, parameters, pragmaName, pragmaFormat);
	const StageSplit split = SplitStages(merged);
	out.parameters = std::move(parameters);
	// Merge parameter names declared by the preset side (.slangp/.params
	// overlays declare parameters without #pragma lines in the slang). The
	// slang's own #pragma declarations win for description/range metadata.
	for (const ParamInfo &ext : presetParameters) {
		bool known = false;
		for (const ParamInfo &p : out.parameters) {
			if (p.name.size() == ext.name.size()
			    && std::equal(p.name.begin(), p.name.end(), ext.name.begin(),
			        [](char a, char b) { return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b)); })) {
				known = true;
				break;
			}
		}
		if (!known) {
			ParamInfo info;
			info.name = ext.name;
			info.defaultValue = ext.defaultValue;
			out.parameters.push_back(info);
		}
	}
	out.pragmaName = std::move(pragmaName);
	out.pragmaFormat = std::move(pragmaFormat);

	if (TrimAll(split.shared).empty() && TrimAll(split.vertex).empty() && TrimAll(split.fragment).empty()) {
		out.error = "slang: shader file has no code";
		return false;
	}

	if (!ProcessStage(split.shared + split.vertex, /*isVertex=*/true, passIndex, passCount, knownTextureNames,
	        out.parameters, out.vertex, out.error)) {
		return false;
	}
	if (!ProcessStage(split.shared + split.fragment, /*isVertex=*/false, passIndex, passCount, knownTextureNames,
	        out.parameters, out.fragment, out.error)) {
		return false;
	}

	// A fragment-only .slang: inject the stock vertex stage.
	if (!out.vertex.present && out.fragment.present) {
		out.vertex.glsl = StockVertexSource();
		out.vertex.present = true;
	}
	if (!out.fragment.present) {
		out.error = "slang: shader file has no fragment stage";
		return false;
	}
	return true;
}

} // namespace preset
} // namespace gpu_scaler
} // namespace devilution
#endif // !USE_SDL1
#endif // USE_GPU_SCALER
