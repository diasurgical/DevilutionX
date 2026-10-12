#include "engine/render/gpu/gpu_scaler.hpp"

#ifdef USE_GPU_SCALER
#ifndef USE_SDL1

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_timer.h>
#include <SDL3/SDL_video.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "engine/render/gpu/gpu_luts.hpp"
#ifdef GPU_SCALER_SPV_GENERATED
#include "engine/render/gpu/gpu_shader_spv.hpp"
#else
#include "engine/render/gpu/gpu_shader_spv_prebuilt.hpp"
#endif
#include "engine/render/gpu/gpu_shaders_hlsl.hpp"
#include "engine/render/gpu/gpu_shaders_msl.hpp"
#include "engine/render/text_render.hpp"
#include "engine/surface.hpp"
#include "utils/log.hpp"
#include "utils/str_cat.hpp"

#ifdef _WIN32
#include <windows.h>
#endif

namespace devilution {
namespace gpu_scaler {
namespace {

constexpr float kSfxClr = 0.5F; // ScaleFX "Threshold" parameter
constexpr float kSfxSaa = 1.0F; // ScaleFX "Filter AA" parameter
constexpr float kSfxScn = 1.0F; // ScaleFX "Filter Corners" parameter

struct alignas(16) FragUniforms {
        float sourceSize[4]; // (w, h, 1/w, 1/h)
        float params[4];
};

enum class Program {
        Vertex,
        HqxIndex,
        HqxBlend,
        SfxPass0,
        SfxPass1,
        SfxPass2,
        SfxPass3,
        SfxPass4,
        Blit,
};

SDL_GPUDevice *gDevice = nullptr;
SDL_Window *gWindow = nullptr;
SDL_GPUShaderFormat gShaderFormat = SDL_GPU_SHADERFORMAT_INVALID;
ShaderFilter gFilter = ShaderFilter::None;

int gGameWidth = 0;
int gGameHeight = 0;
int gPassScale = 0; // scale of the scaledTex target

SDL_GPUShader *gVertexShader = nullptr;
SDL_GPUShader *gFragmentShaders[9] = {};

SDL_GPUGraphicsPipeline *gHqxIndexPipeline = nullptr;
SDL_GPUGraphicsPipeline *gHqxBlendPipeline = nullptr;
SDL_GPUGraphicsPipeline *gSfxPipelines[5] = {};
SDL_GPUGraphicsPipeline *gBlitPipeline = nullptr;

SDL_GPUTexture *gGameTex = nullptr;
SDL_GPUTexture *gIndexTex = nullptr;
SDL_GPUTexture *gMetricTex = nullptr;   // ScaleFX pass 0 (fp16)
SDL_GPUTexture *gStrengthTex = nullptr; // ScaleFX pass 1 (fp16)
SDL_GPUTexture *gFlagsTex = nullptr;    // ScaleFX pass 2
SDL_GPUTexture *gSubpixTex = nullptr;   // ScaleFX pass 3
SDL_GPUTexture *gScaledTex = nullptr;

SDL_GPUSampler *gNearestSampler = nullptr;
SDL_GPUSampler *gLinearSampler = nullptr;
SDL_GPUTexture *gHqxLuts[3] = {}; // HQ2x, HQ3x, HQ4x

bool gLoggedFailure = false;

// Set when the scaler hits an unrecoverable error. Session-sticky: the game
// reverts to the regular renderer instead of retrying a broken GPU path.
bool gFailed = false;
// The last window successfully claimed by the GPU device. Deliberately kept
// after Shutdown(): on Windows the HWND keeps its DXGI flip-model swapchain
// history, so display code must recreate the window before handing it back
// to the regular SDL_Renderer. Only ever compared by pointer, never
// dereferenced (the window may already be destroyed).
SDL_Window *gLastClaimedWindow = nullptr;
// Format the current blit pipeline was built for (swapchain format can change).
SDL_GPUTextureFormat gBlitPipelineFormat = SDL_GPU_TEXTUREFORMAT_INVALID;

// SDL_Log output is invisible for a GUI-subsystem application on Windows, so
// GPU scaler diagnostics are also appended to gpu-scaler.log next to the
// executable. The file opens lazily on first use.
FILE *LogFile();
void InstallSdlLogCapture();

FILE *LogFile()
{
        static FILE *file = nullptr;
        static bool initialized = false;
        if (!initialized) {
                initialized = true;
                const char *basePath = SDL_GetBasePath();
                if (basePath != nullptr) {
                        // SDL3 owns the returned string; it must not be freed.
                        const std::string path = std::string(basePath) + "gpu-scaler.log";
                        file = std::fopen(path.c_str(), "a");
                        if (file != nullptr) {
                                std::fprintf(file, "\n=== GPU scaler session (build %s %s) ===\n", __DATE__, __TIME__);
                                InstallSdlLogCapture();
                        }
                }
        }
        return file;
}

void LogFileLine(std::string_view line)
{
        if (FILE *file = LogFile(); file != nullptr) {
                std::fprintf(file, "[%10u ms] %.*s\n", static_cast<unsigned>(SDL_GetTicks()), static_cast<int>(line.size()), line.data());
                std::fflush(file);
        }
}

void LogOk(std::string_view what)
{
        LogFileLine(StrCat("ok: ", what));
}

void LogFail(std::string_view what)
{
        if (!gLoggedFailure) {
                gLoggedFailure = true;
                LogError("GPU scaler: {}: {} (further GPU scaler errors will be suppressed)", what, SDL_GetError());
        }
        LogFileLine(StrCat("ERROR: ", what, " | SDL_GetError: ", SDL_GetError()));
}

// A failure that cannot be recovered from within the GPU path: the scaler is
// torn down and the game falls back to the regular renderer for this session.
void LogFailHard(std::string_view what)
{
        gFailed = true;
        LogFail(what);
        LogFileLine("GPU scaler disabled for this session (hard failure)");
}

// SDL's own log output (adapter info, validation errors, device removal,
// backend initialization) is invisible for a GUI-subsystem application on
// Windows. Chain it into the diagnostic file as well.
SDL_LogOutputFunction gPrevSdlLogFunction = nullptr;
void *gPrevSdlLogUserdata = nullptr;

void SDLCALL SdlLogCapture(void *userdata, int category, SDL_LogPriority priority, const char *message)
{
        if (gPrevSdlLogFunction != nullptr)
                gPrevSdlLogFunction(gPrevSdlLogUserdata, category, priority, message);
        if (FILE *file = LogFile(); file != nullptr) {
                std::fprintf(file, "[       SDL] %s\n", message != nullptr ? message : "");
                std::fflush(file);
        }
}

void InstallSdlLogCapture()
{
        if (gPrevSdlLogFunction != nullptr)
                return;
        SDL_GetLogOutputFunction(&gPrevSdlLogFunction, &gPrevSdlLogUserdata);
        SDL_SetLogOutputFunction(SdlLogCapture, nullptr);
}

#ifdef _WIN32
// Writes fatal native exceptions (access violations and friends) to the
// diagnostic file before the process dies. Purely diagnostic: the exception
// is always re-raised (EXCEPTION_CONTINUE_SEARCH) so debuggers and WER keep
// working. Guarded exceptions raised internally by drivers or the compiler
// runtime also pass through here; they are counted so a later truly fatal
// crash can still be distinguished.
//
// The handler also records the faulting instruction pointer and a raw stack
// backtrace as offsets from the executable's image base. The executable is
// built with `-g2` (file/line debug info), so these offsets can be resolved
// offline with `addr2line -e devilutionx.exe <offset>` against the exact
// binary that produced the log.
extern "C" USHORT NTAPI RtlCaptureStackBackTrace(
    ULONG FramesToSkip, ULONG FramesToCapture, void **BackTrace, ULONG *BackTraceHash);

std::string HexU64(uint64_t value)
{
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%llX", static_cast<unsigned long long>(value));
        return buf;
}

std::string HexU32(uint32_t value)
{
        return HexU64(value);
}

std::string CrashThreadName()
{
        using GetThreadDescriptionFn = HRESULT(WINAPI *)(HANDLE, PWSTR *);
        static GetThreadDescriptionFn fn = reinterpret_cast<GetThreadDescriptionFn>(
            reinterpret_cast<void *>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "GetThreadDescription")));
        if (fn != nullptr) {
                PWSTR name = nullptr;
                if (fn(GetCurrentThread(), &name) == S_OK && name != nullptr) {
                        std::string result;
                        for (const wchar_t *p = name; *p != L'\0' && result.size() < 32; ++p)
                                result.push_back(static_cast<char>(*p >= L' ' && *p < L'\x7F' ? *p : L'?'));
                        LocalFree(name);
                        return result;
                }
        }
        return "";
}

long NTAPI ScalerCrashHandler(EXCEPTION_POINTERS *info)
{
        static int logged = 0;
        if (info != nullptr && info->ExceptionRecord != nullptr && logged < 8) {
                const DWORD code = info->ExceptionRecord->ExceptionCode;
                const char *what = nullptr;
                switch (code) {
                case 0xC0000005: what = "access violation"; break;
                case 0xC000001D: what = "illegal instruction"; break;
                case 0xC0000094: what = "integer divide by zero"; break;
                case 0xC00000FD: what = "stack overflow"; break;
                case 0xC0000374: what = "heap corruption"; break;
                case 0xC0000409: what = "stack buffer overrun"; break;
                default: break;
                }
                if (what != nullptr) {
                        logged += 1;
                        const std::string threadName = CrashThreadName();
                        std::string crashLine = StrCat(
                            "CRASH #", logged, ": ", what,
                            " (code=0x", HexU32(static_cast<uint32_t>(code)), ", ");
                        if (code == 0xC0000005 && info->ExceptionRecord->NumberParameters >= 2) {
                                crashLine += StrCat(
                                    info->ExceptionRecord->ExceptionInformation[0] == 1 ? "write to" : "read from",
                                    " address 0x", HexU64(static_cast<uint64_t>(info->ExceptionRecord->ExceptionInformation[1])), ", ");
                        }
                        crashLine += StrCat("thread ", GetCurrentThreadId());
                        if (!threadName.empty())
                                crashLine += StrCat(" \"", threadName, "\"");
                        crashLine += ')';
                        LogFileLine(crashLine);

                        // Module-relative offsets for the faulting IP and the raw
                        // call stack. The build is fully static, so nearly all
                        // frames live in the exe itself.
                        const HMODULE exeModule = GetModuleHandleW(nullptr);
                        const uintptr_t exeBase = reinterpret_cast<uintptr_t>(exeModule);
                        uintptr_t exeEnd = exeBase;
                        if (exeModule != nullptr) {
                                const auto *dosHeader = reinterpret_cast<const IMAGE_DOS_HEADER *>(exeModule);
                                if (dosHeader->e_magic == IMAGE_DOS_SIGNATURE) {
                                        const auto *ntHeaders = reinterpret_cast<const IMAGE_NT_HEADERS *>(exeBase + dosHeader->e_lfanew);
                                        exeEnd = exeBase + ntHeaders->OptionalHeader.SizeOfImage;
                                }
                        }
                        auto describeAddress = [exeBase, exeEnd](uintptr_t address) {
                                if (address >= exeBase && address < exeEnd)
                                        return StrCat("devilutionx.exe+0x", HexU64(static_cast<uint64_t>(address - exeBase)));
                                return StrCat("0x", HexU64(static_cast<uint64_t>(address)));
                        };
                        if (info->ContextRecord != nullptr) {
                                LogFileLine(StrCat("  faulting ip: ", describeAddress(reinterpret_cast<uintptr_t>(info->ContextRecord->Rip))));
                        }
                        void *frames[24] = {};
                        const USHORT numFrames = RtlCaptureStackBackTrace(0, 24, frames, nullptr);
                        std::string stack = "  stack:";
                        for (USHORT i = 0; i < numFrames; ++i) {
                                stack += ' ';
                                stack += describeAddress(reinterpret_cast<uintptr_t>(frames[i]));
                        }
                        LogFileLine(stack);
                }
        }
        return EXCEPTION_CONTINUE_SEARCH;
}

void InstallCrashHandler()
{
        static bool installed = false;
        if (!installed) {
                installed = true;
                AddVectoredExceptionHandler(/*First=*/1, &ScalerCrashHandler);
        }
}
#endif // _WIN32

#ifdef _WIN32

// Minimal COM-ish interface matching the ID3DBlob vtable layout.
struct FakeD3DBlob {
        virtual long STDMETHODCALLTYPE QueryInterface(void *, void **) = 0;
        virtual unsigned long STDMETHODCALLTYPE AddRef() = 0;
        virtual unsigned long STDMETHODCALLTYPE Release() = 0;
        virtual void *STDMETHODCALLTYPE GetBufferPointer() = 0;
        virtual size_t STDMETHODCALLTYPE GetBufferSize() = 0;
};

using D3DCompileFn = long(STDMETHODCALLTYPE *)(
    const void *, size_t, const char *, const void *, void *, const char *,
    const char *, unsigned, unsigned, FakeD3DBlob **, FakeD3DBlob **);

D3DCompileFn LoadD3DCompile()
{
        static D3DCompileFn fn = nullptr;
        if (fn != nullptr)
                return fn;
        HMODULE mod = LoadLibraryW(L"d3dcompiler_47.dll");
        if (mod == nullptr)
                mod = LoadLibraryW(L"d3dcompiler_43.dll");
        if (mod == nullptr)
                return nullptr;
        fn = reinterpret_cast<D3DCompileFn>(reinterpret_cast<void *>(GetProcAddress(mod, "D3DCompile")));
        if (fn == nullptr)
                LogError("GPU scaler: found d3dcompiler dll but no D3DCompile export");
        return fn;
}

std::vector<uint8_t> CompileHLSL(const char *source, const char *target)
{
        std::vector<uint8_t> blob;
        D3DCompileFn d3dCompile = LoadD3DCompile();
        if (d3dCompile == nullptr) {
                LogError("GPU scaler: d3dcompiler not found, cannot compile shaders for D3D12");
                return blob;
        }
        FakeD3DBlob *code = nullptr;
        FakeD3DBlob *errors = nullptr;
        const long hr = d3dCompile(source, std::strlen(source), nullptr, nullptr, nullptr,
            "main", target, 0, 0, &code, &errors);
        if (hr < 0 || code == nullptr) {
                std::string msg;
                if (errors != nullptr) {
                        msg.assign(static_cast<const char *>(errors->GetBufferPointer()),
                            std::min<size_t>(errors->GetBufferSize(), 400));
                        errors->Release();
                }
                for (char &c : msg) {
                        if (c == '\n' || c == '\r')
                                c = ' ';
                }
                LogError("GPU scaler: HLSL compilation failed ({}): {}", target, msg);
                // Also record it in the diagnostic file: SDL_Log is invisible
                // for GUI applications on Windows.
                LogFileLine(StrCat("ERROR: HLSL compilation failed (", target, "): ", msg));
                return blob;
        }
        const auto *bytes = static_cast<const uint8_t *>(code->GetBufferPointer());
        blob.assign(bytes, bytes + code->GetBufferSize());
        code->Release();
        return blob;
}

#endif // _WIN32

struct ShaderCode {
        const uint8_t *data = nullptr;
        size_t size = 0;
};

ShaderCode SpvFor(Program program)
{
        switch (program) {
        case Program::Vertex: return { gpu::spv::kVertex, sizeof(gpu::spv::kVertex) };
        case Program::HqxIndex: return { gpu::spv::kHqxIndex, sizeof(gpu::spv::kHqxIndex) };
        case Program::HqxBlend: return { gpu::spv::kHqxBlend, sizeof(gpu::spv::kHqxBlend) };
        case Program::SfxPass0: return { gpu::spv::kSfxPass0, sizeof(gpu::spv::kSfxPass0) };
        case Program::SfxPass1: return { gpu::spv::kSfxPass1, sizeof(gpu::spv::kSfxPass1) };
        case Program::SfxPass2: return { gpu::spv::kSfxPass2, sizeof(gpu::spv::kSfxPass2) };
        case Program::SfxPass3: return { gpu::spv::kSfxPass3, sizeof(gpu::spv::kSfxPass3) };
        case Program::SfxPass4: return { gpu::spv::kSfxPass4, sizeof(gpu::spv::kSfxPass4) };
        case Program::Blit: return { gpu::spv::kBlit, sizeof(gpu::spv::kBlit) };
        }
        return { nullptr, 0 };
}

[[maybe_unused]] const char *HlslFor(Program program)
{
#ifndef _WIN32
        // Only used by the D3D12 backend.
        (void)program;
        return nullptr;
#else
        switch (program) {
        case Program::Vertex: return gpu::hlsl::kVertex;
        case Program::HqxIndex: return gpu::hlsl::kHqxIndex;
        case Program::HqxBlend: return gpu::hlsl::kHqxBlend;
        case Program::SfxPass0: return gpu::hlsl::kSfxPass0;
        case Program::SfxPass1: return gpu::hlsl::kSfxPass1;
        case Program::SfxPass2: return gpu::hlsl::kSfxPass2;
        case Program::SfxPass3: return gpu::hlsl::kSfxPass3;
        case Program::SfxPass4: return gpu::hlsl::kSfxPass4;
        case Program::Blit: return gpu::hlsl::kBlit;
        }
        return nullptr;
#endif
}

const char *MslFor(Program program)
{
        switch (program) {
        case Program::Vertex: return gpu::msl::kVertex;
        case Program::HqxIndex: return gpu::msl::kHqxIndex;
        case Program::HqxBlend: return gpu::msl::kHqxBlend;
        case Program::SfxPass0: return gpu::msl::kSfxPass0;
        case Program::SfxPass1: return gpu::msl::kSfxPass1;
        case Program::SfxPass2: return gpu::msl::kSfxPass2;
        case Program::SfxPass3: return gpu::msl::kSfxPass3;
        case Program::SfxPass4: return gpu::msl::kSfxPass4;
        case Program::Blit: return gpu::msl::kBlit;
        }
        return nullptr;
}

int SamplersFor(Program program)
{
        switch (program) {
        case Program::Vertex: return 0;
        case Program::HqxIndex: return 1;
        case Program::HqxBlend: return 3;
        case Program::SfxPass0: return 1;
        case Program::SfxPass1: return 1;
        case Program::SfxPass2: return 2;
        case Program::SfxPass3: return 1;
        case Program::SfxPass4: return 2;
        case Program::Blit: return 1;
        }
        return 0;
}

const char *NameFor(Program program)
{
        switch (program) {
        case Program::Vertex: return "vertex";
        case Program::HqxIndex: return "hqx-index";
        case Program::HqxBlend: return "hqx-blend";
        case Program::SfxPass0: return "sfx-pass0";
        case Program::SfxPass1: return "sfx-pass1";
        case Program::SfxPass2: return "sfx-pass2";
        case Program::SfxPass3: return "sfx-pass3";
        case Program::SfxPass4: return "sfx-pass4";
        case Program::Blit: return "blit";
        }
        return "unknown";
}

const char *MslEntryFor(Program program)
{
        switch (program) {
        case Program::Vertex: return "fullscreenVS";
        case Program::HqxIndex: return "hqxIndexPS";
        case Program::HqxBlend: return "hqxBlendPS";
        case Program::SfxPass0: return "sfxPass0PS";
        case Program::SfxPass1: return "sfxPass1PS";
        case Program::SfxPass2: return "sfxPass2PS";
        case Program::SfxPass3: return "sfxPass3PS";
        case Program::SfxPass4: return "sfxPass4PS";
        case Program::Blit: return "blitPS";
        }
        return "main";
}

bool CreateShader(Program program, SDL_GPUShaderStage stage, SDL_GPUShader **out)
{
        SDL_GPUShaderCreateInfo info {};
        info.stage = stage;
        info.entrypoint = (gShaderFormat == SDL_GPU_SHADERFORMAT_MSL) ? MslEntryFor(program) : "main";
        info.num_samplers = (stage == SDL_GPU_SHADERSTAGE_FRAGMENT) ? SamplersFor(program) : 0;
        info.num_uniform_buffers = (stage == SDL_GPU_SHADERSTAGE_FRAGMENT) ? 1 : 0;

        // Must outlive the SDL_CreateGPUShader call below.
        std::vector<uint8_t> dxbcBlob;

        switch (gShaderFormat) {
        case SDL_GPU_SHADERFORMAT_SPIRV: {
                const ShaderCode code = SpvFor(program);
                info.code = code.data;
                info.code_size = code.size;
                info.format = SDL_GPU_SHADERFORMAT_SPIRV;
        } break;
        case SDL_GPU_SHADERFORMAT_MSL: {
                const char *source = MslFor(program);
                info.code = reinterpret_cast<const uint8_t *>(source);
                info.code_size = std::strlen(source);
                info.format = SDL_GPU_SHADERFORMAT_MSL;
        } break;
        case SDL_GPU_SHADERFORMAT_DXBC: {
#ifdef _WIN32
                const char *source = HlslFor(program);
                // Shader Model 5.0 first: supported by every d3dcompiler and the
                // least exotic input for GPU drivers. 5.1 only as a fallback.
                const char *target = stage == SDL_GPU_SHADERSTAGE_VERTEX ? "vs_5_0" : "ps_5_0";
                LogFileLine(StrCat("compiling shader '", NameFor(program), "' (", target, ") via d3dcompiler"));
                dxbcBlob = CompileHLSL(source, target);
                if (dxbcBlob.empty()) {
                        dxbcBlob = CompileHLSL(source, stage == SDL_GPU_SHADERSTAGE_VERTEX ? "vs_5_1" : "ps_5_1");
                        if (dxbcBlob.empty()) {
                                LogFileLine(StrCat("ERROR: all shader targets failed for '", NameFor(program), "'"));
                                return false;
                        }
                }
                LogFileLine(StrCat("compiled shader '", NameFor(program), "', bytes=", dxbcBlob.size()));
                info.code = dxbcBlob.data();
                info.code_size = dxbcBlob.size();
                info.format = SDL_GPU_SHADERFORMAT_DXBC;
                break;
#else
                return false;
#endif
        }
        default:
                return false;
        }

        *out = SDL_CreateGPUShader(gDevice, &info);
        if (*out == nullptr) {
                LogFail(StrCat("shader creation failed ('", NameFor(program), "')"));
                return false;
        }
        LogOk(StrCat("shader '", NameFor(program), "' created"));
        return true;
}

SDL_GPUTexture *CreateTexture(SDL_GPUTextureFormat format, Uint32 width, Uint32 height, SDL_GPUTextureUsageFlags usage)
{
        SDL_GPUTextureCreateInfo info {};
        info.type = SDL_GPU_TEXTURETYPE_2D;
        info.format = format;
        info.usage = usage;
        info.width = width;
        info.height = height;
        info.layer_count_or_depth = 1;
        info.num_levels = 1;
        info.sample_count = SDL_GPU_SAMPLECOUNT_1;
        SDL_GPUTexture *texture = SDL_CreateGPUTexture(gDevice, &info);
        if (texture == nullptr)
                LogFail("texture creation failed");
        return texture;
}

SDL_GPUTexture *CreateHqxLut(const gpu::luts::LutData &lut, const char *label)
{
        std::vector<uint8_t> rgba8(static_cast<size_t>(lut.width) * lut.height * 4);
        for (int i = 0; i < lut.width * lut.height; ++i) {
                const uint8_t idx = lut.indices[i];
                rgba8[4 * i + 0] = lut.palette[idx][0];
                rgba8[4 * i + 1] = lut.palette[idx][1];
                rgba8[4 * i + 2] = lut.palette[idx][2];
                rgba8[4 * i + 3] = lut.palette[idx][3];
        }
        SDL_GPUTexture *texture = CreateTexture(SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, lut.width, lut.height,
            SDL_GPU_TEXTUREUSAGE_SAMPLER);
        if (texture == nullptr)
                return nullptr;

        SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(gDevice);
        if (cmd == nullptr) {
                LogFail("command buffer acquisition failed");
                SDL_ReleaseGPUTexture(gDevice, texture);
                return nullptr;
        }
        SDL_GPUTransferBufferCreateInfo transferInfo {};
        transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        transferInfo.size = static_cast<Uint32>(rgba8.size());
        SDL_GPUTransferBuffer *transfer = SDL_CreateGPUTransferBuffer(gDevice, &transferInfo);
        if (transfer == nullptr) {
                LogFail("transfer buffer creation failed");
                SDL_ReleaseGPUTexture(gDevice, texture);
                return nullptr;
        }
        auto *data = static_cast<uint8_t *>(SDL_MapGPUTransferBuffer(gDevice, transfer, false));
        std::memcpy(data, rgba8.data(), rgba8.size());
        SDL_UnmapGPUTransferBuffer(gDevice, transfer);

        SDL_GPUCopyPass *copyPass = SDL_BeginGPUCopyPass(cmd);
        SDL_GPUTextureTransferInfo src {};
        src.transfer_buffer = transfer;
        src.offset = 0;
        // texels are tightly packed in the transfer buffer
        src.pixels_per_row = lut.width;
        src.rows_per_layer = lut.height;
        SDL_GPUTextureRegion dst {};
        dst.texture = texture;
        dst.w = lut.width;
        dst.h = lut.height;
        dst.d = 1;
        SDL_UploadToGPUTexture(copyPass, &src, &dst, false);
        SDL_EndGPUCopyPass(copyPass);
        SDL_SubmitGPUCommandBuffer(cmd);
        SDL_ReleaseGPUTransferBuffer(gDevice, transfer);
        LogOk(StrCat("hqx LUT '", label, "' queued for upload"));
        return texture;
}

SDL_GPUTextureFormat GameTextureFormat()
{
        switch (DEVILUTIONX_DISPLAY_TEXTURE_FORMAT) {
        case SDL_PIXELFORMAT_ABGR8888:
                return SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
        case SDL_PIXELFORMAT_ARGB8888:
        case SDL_PIXELFORMAT_XRGB8888:
        default:
                return SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;
        }
}

SDL_GPUGraphicsPipeline *CreatePipeline(Program fragmentProgram, SDL_GPUTextureFormat targetFormat)
{
        if (gVertexShader == nullptr)
                return nullptr;
        SDL_GPUShader *fragmentShader = gFragmentShaders[static_cast<int>(fragmentProgram)];
        if (fragmentShader == nullptr)
                return nullptr;
        if (targetFormat == SDL_GPU_TEXTUREFORMAT_INVALID)
                return nullptr;

        SDL_GPUColorTargetDescription targetDesc {};
        targetDesc.format = targetFormat;

        SDL_GPUGraphicsPipelineCreateInfo info {};
        info.vertex_shader = gVertexShader;
        info.fragment_shader = fragmentShader;
        info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
        info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
        info.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_1;
        info.multisample_state.sample_mask = 0xFFFFFFFF;
        info.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_ALWAYS;
        info.depth_stencil_state.front_stencil_state.fail_op = SDL_GPU_STENCILOP_KEEP;
        info.depth_stencil_state.front_stencil_state.pass_op = SDL_GPU_STENCILOP_KEEP;
        info.depth_stencil_state.front_stencil_state.depth_fail_op = SDL_GPU_STENCILOP_KEEP;
        info.depth_stencil_state.front_stencil_state.compare_op = SDL_GPU_COMPAREOP_ALWAYS;
        info.depth_stencil_state.back_stencil_state = info.depth_stencil_state.front_stencil_state;
        info.target_info.color_target_descriptions = &targetDesc;
        info.target_info.num_color_targets = 1;

        SDL_GPUGraphicsPipeline *pipeline = SDL_CreateGPUGraphicsPipeline(gDevice, &info);
        if (pipeline == nullptr)
                LogFail(StrCat("pipeline creation failed ('", NameFor(fragmentProgram), "')"));
        else
                LogOk(StrCat("pipeline '", NameFor(fragmentProgram), "' created"));
        return pipeline;
}

bool EnsureSamplers()
{
        if (gNearestSampler != nullptr)
                return true;

        SDL_GPUSamplerCreateInfo samplerInfo {};
        samplerInfo.min_filter = SDL_GPU_FILTER_NEAREST;
        samplerInfo.mag_filter = SDL_GPU_FILTER_NEAREST;
        samplerInfo.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
        samplerInfo.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        samplerInfo.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        samplerInfo.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        samplerInfo.max_anisotropy = 1.0F;
        samplerInfo.min_lod = 0.0F;
        samplerInfo.max_lod = 1000.0F;
        gNearestSampler = SDL_CreateGPUSampler(gDevice, &samplerInfo);

        SDL_GPUSamplerCreateInfo linearInfo = samplerInfo;
        linearInfo.min_filter = SDL_GPU_FILTER_LINEAR;
        linearInfo.mag_filter = SDL_GPU_FILTER_LINEAR;
        gLinearSampler = SDL_CreateGPUSampler(gDevice, &linearInfo);

        if (gNearestSampler == nullptr || gLinearSampler == nullptr) {
                LogFail("sampler creation failed");
                return false;
        }
        return true;
}

bool EnsureBlitPipeline(SDL_GPUTextureFormat format)
{
        if (gBlitPipeline != nullptr && gBlitPipelineFormat == format)
                return true;
        if (gBlitPipeline != nullptr) {
                SDL_ReleaseGPUGraphicsPipeline(gDevice, gBlitPipeline);
                gBlitPipeline = nullptr;
                gBlitPipelineFormat = SDL_GPU_TEXTUREFORMAT_INVALID;
        }
        gBlitPipeline = CreatePipeline(Program::Blit, format);
        if (gBlitPipeline == nullptr) {
                LogFail("blit pipeline creation failed");
                return false;
        }
        gBlitPipelineFormat = format;
        LogFileLine(StrCat("blit pipeline ready, swapchain format=", static_cast<int>(format)));
        return true;
}

void ReleaseFilterResources()
{
        gBlitPipelineFormat = SDL_GPU_TEXTUREFORMAT_INVALID;
        if (gVertexShader != nullptr) {
                SDL_ReleaseGPUShader(gDevice, gVertexShader);
                gVertexShader = nullptr;
        }
        for (auto *&shader : gFragmentShaders) {
                if (shader != nullptr) {
                        SDL_ReleaseGPUShader(gDevice, shader);
                        shader = nullptr;
                }
        }
        if (gHqxIndexPipeline != nullptr) {
                SDL_ReleaseGPUGraphicsPipeline(gDevice, gHqxIndexPipeline);
                gHqxIndexPipeline = nullptr;
        }
        if (gHqxBlendPipeline != nullptr) {
                SDL_ReleaseGPUGraphicsPipeline(gDevice, gHqxBlendPipeline);
                gHqxBlendPipeline = nullptr;
        }
        for (auto *&pipeline : gSfxPipelines) {
                if (pipeline != nullptr) {
                        SDL_ReleaseGPUGraphicsPipeline(gDevice, pipeline);
                        pipeline = nullptr;
                }
        }
        if (gBlitPipeline != nullptr) {
                SDL_ReleaseGPUGraphicsPipeline(gDevice, gBlitPipeline);
                gBlitPipeline = nullptr;
        }
        for (auto *&lut : gHqxLuts) {
                if (lut != nullptr) {
                        SDL_ReleaseGPUTexture(gDevice, lut);
                        lut = nullptr;
                }
        }
}

bool EnsureShaders()
{
        if (gVertexShader != nullptr)
                return true;
        if (!CreateShader(Program::Vertex, SDL_GPU_SHADERSTAGE_VERTEX, &gVertexShader))
                return false;
        static constexpr Program kFragmentPrograms[] = {
                Program::HqxIndex, Program::HqxBlend, Program::SfxPass0, Program::SfxPass1,
                Program::SfxPass2, Program::SfxPass3, Program::SfxPass4, Program::Blit
        };
        for (const Program program : kFragmentPrograms) {
                if (!CreateShader(program, SDL_GPU_SHADERSTAGE_FRAGMENT, &gFragmentShaders[static_cast<int>(program)]))
                        return false;
        }
        return true;
}

bool EnsureFilterResources(ShaderFilter filter)
{
        if (!EnsureSamplers() || !EnsureShaders())
                return false;
        LogOk("samplers and shaders ready");

        const bool isHqx = filter == ShaderFilter::HQ2x || filter == ShaderFilter::HQ3x || filter == ShaderFilter::HQ4x;
        const bool isSfx = IsScaleFX(filter);

        if (isHqx && gHqxIndexPipeline == nullptr) {
                gHqxIndexPipeline = CreatePipeline(Program::HqxIndex, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM);
                if (gHqxIndexPipeline == nullptr)
                        return false;
        }
        if (isHqx && gHqxBlendPipeline == nullptr) {
                gHqxBlendPipeline = CreatePipeline(Program::HqxBlend, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM);
                if (gHqxBlendPipeline == nullptr)
                        return false;
                for (int i = 0; i < 3; ++i) {
                        const gpu::luts::LutData *lutData = i == 0 ? &gpu::luts::HQ2x : i == 1 ? &gpu::luts::HQ3x : &gpu::luts::HQ4x;
                        gHqxLuts[i] = CreateHqxLut(*lutData, i == 0 ? "hq2x" : i == 1 ? "hq3x" : "hq4x");
                        if (gHqxLuts[i] == nullptr)
                                return false;
                }
                LogOk("hqx pipelines and LUTs ready");
        }
        if (isSfx && gSfxPipelines[0] == nullptr) {
                static constexpr std::pair<Program, SDL_GPUTextureFormat> kSfxPipelinesSpec[5] = {
                        { Program::SfxPass0, SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT },
                        { Program::SfxPass1, SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT },
                        { Program::SfxPass2, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM },
                        { Program::SfxPass3, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM },
                        { Program::SfxPass4, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM },
                };
                for (int i = 0; i < 5; ++i) {
                        gSfxPipelines[i] = CreatePipeline(kSfxPipelinesSpec[i].first, kSfxPipelinesSpec[i].second);
                        if (gSfxPipelines[i] == nullptr)
                                return false;
                }
                LogOk("scalefx pipelines ready");
        }
        if (gBlitPipeline == nullptr) {
                const SDL_GPUTextureFormat swapchainFormat = SDL_GetGPUSwapchainTextureFormat(gDevice, gWindow);
                if (swapchainFormat == SDL_GPU_TEXTUREFORMAT_INVALID) {
                        // The swapchain is not ready yet (claimed but never
                        // presented to). Build the blit pipeline lazily on the
                        // first present instead of guessing the format here.
                        LogFileLine("swapchain format unavailable at init; deferring blit pipeline creation");
                        return true;
                }
                if (!EnsureBlitPipeline(swapchainFormat))
                        return false;
        }
        return true;
}

void ReleaseFrameResources()
{
        for (SDL_GPUTexture **texture : { &gGameTex, &gIndexTex, &gMetricTex, &gStrengthTex, &gFlagsTex, &gSubpixTex, &gScaledTex }) {
                if (*texture != nullptr) {
                        SDL_ReleaseGPUTexture(gDevice, *texture);
                        *texture = nullptr;
                }
        }
        gGameWidth = 0;
        gGameHeight = 0;
        gPassScale = 0;
}

bool EnsureFrameResources(int gameWidth, int gameHeight, int passScale)
{
        if (gGameTex != nullptr && gGameWidth == gameWidth && gGameHeight == gameHeight
            && (gScaledTex == nullptr || gPassScale == passScale)) {
                return true;
        }
        ReleaseFrameResources();
        gGameWidth = gameWidth;
        gGameHeight = gameHeight;
        gPassScale = passScale;

        const SDL_GPUTextureFormat gameFormat = GameTextureFormat();
        gGameTex = CreateTexture(gameFormat, gameWidth, gameHeight,
            SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET);
        gIndexTex = CreateTexture(SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, gameWidth, gameHeight,
            SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET);
        gMetricTex = CreateTexture(SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT, gameWidth, gameHeight,
            SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET);
        gStrengthTex = CreateTexture(SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT, gameWidth, gameHeight,
            SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET);
        gFlagsTex = CreateTexture(SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, gameWidth, gameHeight,
            SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET);
        gSubpixTex = CreateTexture(SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, gameWidth, gameHeight,
            SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET);
        const int scaledWidth = gameWidth * passScale;
        const int scaledHeight = gameHeight * passScale;
        gScaledTex = CreateTexture(SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, scaledWidth, scaledHeight,
            SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET);

        if (gGameTex == nullptr || gIndexTex == nullptr || gMetricTex == nullptr || gStrengthTex == nullptr
            || gFlagsTex == nullptr || gSubpixTex == nullptr || gScaledTex == nullptr) {
                LogFailHard("frame resource creation failed");
                ReleaseFrameResources();
                return false;
        }
        return true;
}

bool RenderPass(SDL_GPUCommandBuffer *cmd, SDL_GPUTexture *target, Uint32 targetWidth, Uint32 targetHeight,
    SDL_GPUGraphicsPipeline *pipeline, const FragUniforms &uniforms,
    const SDL_GPUTextureSamplerBinding *bindings, Uint32 numBindings,
    const SDL_GPUViewport *viewport)
{
        SDL_GPUColorTargetInfo colorInfo {};
        colorInfo.texture = target;
        colorInfo.clear_color = { 0.0F, 0.0F, 0.0F, 1.0F };
        colorInfo.load_op = SDL_GPU_LOADOP_CLEAR;
        colorInfo.store_op = SDL_GPU_STOREOP_STORE;
        SDL_GPURenderPass *renderPass = SDL_BeginGPURenderPass(cmd, &colorInfo, 1, nullptr);
        if (renderPass == nullptr) {
                LogFailHard("render pass creation failed");
                return false;
        }
        SDL_BindGPUGraphicsPipeline(renderPass, pipeline);
        if (numBindings > 0)
                SDL_BindGPUFragmentSamplers(renderPass, 0, bindings, numBindings);
        SDL_PushGPUFragmentUniformData(cmd, 0, &uniforms, sizeof(uniforms));
        if (viewport != nullptr)
                SDL_SetGPUViewport(renderPass, viewport);
        SDL_DrawGPUPrimitives(renderPass, 3, 1, 0, 0);
        SDL_EndGPURenderPass(renderPass);
        return true;
}

} // namespace

void LogPhase(std::string_view line)
{
        LogFileLine(line);
}

bool ScalerWanted()
{
        return *GetOptions().Graphics.shaderFilter != ShaderFilter::None;
}

bool IsActive()
{
        return gDevice != nullptr && gWindow != nullptr && gFilter != ShaderFilter::None;
}

bool HasFailed()
{
        return gFailed;
}

bool WindowHasSwapchainHistory(SDL_Window *window)
{
        return window != nullptr && gLastClaimedWindow == window;
}

// Creates the GPU device and selects the shader format for its backend.
// `name` forces a specific backend (e.g. "vulkan") or auto-selects when null.
// The DEVILUTIONX_GPU_BACKEND environment variable overrides both, which is
// useful for troubleshooting a broken D3D12 or Vulkan driver.
static bool CreateDevice(const char *name)
{
        const char *backendOverride = SDL_getenv("DEVILUTIONX_GPU_BACKEND");
        if (backendOverride != nullptr && *backendOverride == '\0')
                backendOverride = nullptr;
        if (backendOverride != nullptr)
                LogFileLine(StrCat("backend override from DEVILUTIONX_GPU_BACKEND: ", backendOverride));

        SDL_PropertiesID props = SDL_CreateProperties();
        SDL_SetBooleanProperty(props, SDL_PROP_GPU_DEVICE_CREATE_DEBUGMODE_BOOLEAN, false);
        SDL_SetBooleanProperty(props, SDL_PROP_GPU_DEVICE_CREATE_SHADERS_SPIRV_BOOLEAN, true);
        SDL_SetBooleanProperty(props, SDL_PROP_GPU_DEVICE_CREATE_SHADERS_DXBC_BOOLEAN, true);
        SDL_SetBooleanProperty(props, SDL_PROP_GPU_DEVICE_CREATE_SHADERS_MSL_BOOLEAN, true);
        // Ask SDL to log the adapter name and driver version; the log capture
        // installed below redirects these messages into the diagnostic file.
        SDL_SetBooleanProperty(props, SDL_PROP_GPU_DEVICE_CREATE_VERBOSE_BOOLEAN, true);
        if (backendOverride != nullptr || name != nullptr)
                SDL_SetStringProperty(props, SDL_PROP_GPU_DEVICE_CREATE_NAME_STRING,
                    backendOverride != nullptr ? backendOverride : name);
        gDevice = SDL_CreateGPUDeviceWithProperties(props);
        SDL_DestroyProperties(props);
        if (gDevice == nullptr)
                return false;
        const char *driver = SDL_GetGPUDeviceDriver(gDevice);
        if (driver != nullptr && std::strcmp(driver, "vulkan") == 0) {
                gShaderFormat = SDL_GPU_SHADERFORMAT_SPIRV;
        } else if (driver != nullptr && std::strcmp(driver, "direct3d12") == 0) {
#ifdef _WIN32
                gShaderFormat = SDL_GPU_SHADERFORMAT_DXBC;
#else
                LogFailHard("D3D12 backend only supported on Windows");
                Shutdown();
                return false;
#endif
        } else if (driver != nullptr && std::strcmp(driver, "metal") == 0) {
                gShaderFormat = SDL_GPU_SHADERFORMAT_MSL;
        } else {
                LogFailHard(StrCat("unsupported GPU driver: ", driver != nullptr ? driver : "unknown"));
                Shutdown();
                return false;
        }
        Log("GPU scaler: using the {} backend", driver);
        LogFileLine(StrCat("device created, backend=", driver != nullptr ? driver : "unknown"));
        return true;
}

bool Reinit(SDL_Window *window)
{
#ifdef _WIN32
        InstallCrashHandler();
#endif
        if (gFailed) {
                LogFileLine("Reinit skipped: the scaler failed earlier this session");
                gFilter = ShaderFilter::None;
                return false;
        }
        gFilter = *GetOptions().Graphics.shaderFilter;
        if (gFilter == ShaderFilter::None) {
                Shutdown();
                return false;
        }
        LogFileLine(StrCat("Reinit: filter=", static_cast<int>(gFilter), " (0=Off, 1-3=HQ2x/3x/4x, 4-6=ScaleFX 2x/3x/4x)"));

        if (gDevice == nullptr) {
                if (!CreateDevice(/*name=*/nullptr)) {
                        LogFailHard("SDL_CreateGPUDevice failed");
                        gFilter = ShaderFilter::None;
                        return false;
                }
        }

        if (gWindow != window) {
                if (gWindow != nullptr)
                        SDL_ReleaseWindowFromGPUDevice(gDevice, gWindow);
                gWindow = window;
                bool claimed = SDL_ClaimWindowForGPUDevice(gDevice, gWindow);
#ifdef _WIN32
                if (!claimed && gShaderFormat == SDL_GPU_SHADERFORMAT_DXBC) {
                        // DXGI can refuse the D3D12 swapchain creation even on a
                        // freshly recreated window (driver quirk / flip-model
                        // ownership rules). Retry once with the Vulkan backend
                        // before giving up; the shaders ship with prebuilt
                        // SPIR-V blobs, so no recompilation is needed.
                        LogFileLine("claim failed on direct3d12, retrying with the vulkan backend");
                        Shutdown();
                        gWindow = window;
                        gFilter = *GetOptions().Graphics.shaderFilter;
                        if (CreateDevice(/*name=*/"vulkan"))
                                claimed = SDL_ClaimWindowForGPUDevice(gDevice, gWindow);
                }
#endif
                if (!claimed) {
                        LogFailHard("SDL_ClaimWindowForGPUDevice failed");
                        gWindow = nullptr;
                        Shutdown();
                        return false;
                }
                gLastClaimedWindow = gWindow;
                const bool vsync = *GetOptions().Graphics.frameRateControl == FrameRateControl::VerticalSync;
                if (!SDL_SetGPUSwapchainParameters(gDevice, gWindow, SDL_GPU_SWAPCHAINCOMPOSITION_SDR,
                        vsync ? SDL_GPU_PRESENTMODE_VSYNC : SDL_GPU_PRESENTMODE_IMMEDIATE)) {
                        LogFail("SDL_SetGPUSwapchainParameters failed");
                }
                LogFileLine(StrCat("window claimed, swapchain format=", static_cast<int>(SDL_GetGPUSwapchainTextureFormat(gDevice, gWindow)),
                    " vsync=", vsync ? "1" : "0"));
                // The swapchain format may have changed with the new window.
                if (gBlitPipeline != nullptr) {
                        SDL_ReleaseGPUGraphicsPipeline(gDevice, gBlitPipeline);
                        gBlitPipeline = nullptr;
                        gBlitPipelineFormat = SDL_GPU_TEXTUREFORMAT_INVALID;
                }
        }

        if (!EnsureFilterResources(gFilter)) {
                LogFailHard("filter resource initialization failed");
                Shutdown();
                return false;
        }
        return true;
}

void Shutdown()
{
        LogFileLine("Shutdown");
        if (gDevice == nullptr) {
                gWindow = nullptr;
                gFilter = ShaderFilter::None;
                return;
        }
        ReleaseFrameResources();
        ReleaseFilterResources();
        if (gNearestSampler != nullptr) {
                SDL_ReleaseGPUSampler(gDevice, gNearestSampler);
                gNearestSampler = nullptr;
        }
        if (gLinearSampler != nullptr) {
                SDL_ReleaseGPUSampler(gDevice, gLinearSampler);
                gLinearSampler = nullptr;
        }
        if (gWindow != nullptr) {
                SDL_ReleaseWindowFromGPUDevice(gDevice, gWindow);
                gWindow = nullptr;
        }
        SDL_DestroyGPUDevice(gDevice);
        gDevice = nullptr;
        gShaderFormat = SDL_GPU_SHADERFORMAT_INVALID;
        gBlitPipelineFormat = SDL_GPU_TEXTUREFORMAT_INVALID;
        gFilter = ShaderFilter::None;
        // gLastClaimedWindow is intentionally NOT cleared: the (possibly still
        // alive) window keeps its DXGI swapchain history on Windows.
}

void PresentFrame(const SDL_Surface &frame)
{
        if (!IsActive())
                return;

        // Heartbeat: lets a remote log show whether the present loop is alive
        // (UI responsive) or stalled (main thread stuck elsewhere), and counts
        // frames skipped due to an unavailable swapchain image.
        static Uint32 sLastHeartbeat = 0;
        static unsigned long sHeartbeatFrames = 0;
        static unsigned long sSkippedFrames = 0;
        static unsigned long sTotalFrames = 0;
        ++sHeartbeatFrames;
        ++sTotalFrames;
        {
                const Uint32 now = SDL_GetTicks();
                if (sLastHeartbeat == 0)
                        sLastHeartbeat = now;
                if (now - sLastHeartbeat >= 5000) {
                        LogFileLine(StrCat("heartbeat: ", sHeartbeatFrames, " frames (", sSkippedFrames,
                                        " skipped, ", sTotalFrames, " total, filter=",
                                        static_cast<int>(gFilter), ")"));
                        sLastHeartbeat = now;
                        sHeartbeatFrames = 0;
                        sSkippedFrames = 0;
                }
        }

        static bool loggedFirstPresentStart = false;
        if (!loggedFirstPresentStart) {
                loggedFirstPresentStart = true;
                LogFileLine("present: first frame, acquiring command buffer and swapchain");
        }

        const ShaderFilter filter = gFilter;
        const int baseScale = FilterScale(filter);

        SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(gDevice);
        if (cmd == nullptr) {
                LogFailHard("SDL_AcquireGPUCommandBuffer failed");
                return;
        }

        SDL_GPUTexture *swapchainTexture = nullptr;
        Uint32 swapchainWidth = 0;
        Uint32 swapchainHeight = 0;
        // Non-blocking on purpose: the blocking acquire variant can stall the
        // main thread indefinitely (minimized window, resize, driver quirk),
        // which looks like a frozen UI while audio keeps playing. Skipping a
        // frame costs nothing; the next frame retries.
        if (!SDL_AcquireGPUSwapchainTexture(cmd, gWindow, &swapchainTexture, &swapchainWidth, &swapchainHeight)
            || swapchainTexture == nullptr) {
                // Window minimized or no swapchain image available this frame.
                ++sSkippedFrames;
                SDL_CancelGPUCommandBuffer(cmd);
                return;
        }

        const int gameWidth = frame.w;
        const int gameHeight = frame.h;

        static bool loggedFirstPresent = false;
        if (!loggedFirstPresent) {
                loggedFirstPresent = true;
                LogFileLine(StrCat("first present: game=", gameWidth, "x", gameHeight,
                    " swapchain=", swapchainWidth, "x", swapchainHeight));
        }

        // The swapchain format can change (window resize, fullscreen switch).
        // Rebuild the blit pipeline for the current format when needed.
        if (!EnsureBlitPipeline(SDL_GetGPUSwapchainTextureFormat(gDevice, gWindow))) {
                SDL_CancelGPUCommandBuffer(cmd);
                return;
        }

        // Fit the game frame into the window, honoring the Integer Scaling option.
        float fitScale = std::min(static_cast<float>(swapchainWidth) / gameWidth,
            static_cast<float>(swapchainHeight) / gameHeight);
        if (*GetOptions().Graphics.integerScaling)
                fitScale = std::floor(fitScale);

        int passScale;
        float destScale;
        if (fitScale >= baseScale) {
                passScale = baseScale;
                destScale = baseScale;
        } else if (fitScale >= 1.0F) {
                if (IsScaleFX(filter)) {
                        // ScaleFX supports arbitrary integer scales via a uniform.
                        passScale = std::max(1, static_cast<int>(fitScale));
                        destScale = passScale;
                } else {
                        // hqx is tied to its native LUT scale; render at native scale
                        // and let the blit pass downscale.
                        passScale = baseScale;
                        destScale = std::floor(fitScale);
                }
        } else {
                // Window is smaller than the game resolution; render at the native
                // scale and downscale in the blit pass.
                passScale = baseScale;
                destScale = fitScale;
        }

        if (!EnsureFrameResources(gameWidth, gameHeight, passScale)) {
                SDL_CancelGPUCommandBuffer(cmd);
                return;
        }
        // Upload the frame through a fresh transfer buffer created per frame.
        // The buffer is released when this function returns (RAII below); SDL
        // defers the actual destruction until the GPU is done with it. This
        // avoids overwriting a long-lived buffer that the GPU may still be
        // reading from on the previous frame's copy pass.
        SDL_GPUTransferBufferCreateInfo transferInfo {};
        transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        transferInfo.size = static_cast<Uint32>(gameWidth) * gameHeight * 4;
        SDL_GPUTransferBuffer *transfer = SDL_CreateGPUTransferBuffer(gDevice, &transferInfo);
        if (transfer == nullptr) {
                LogFailHard("frame transfer buffer creation failed");
                SDL_CancelGPUCommandBuffer(cmd);
                return;
        }
        struct TransferBufferReleaser {
                SDL_GPUDevice *device;
                SDL_GPUTransferBuffer *buffer;
                ~TransferBufferReleaser()
                {
                        SDL_ReleaseGPUTransferBuffer(device, buffer);
                }
        } transferReleaser { gDevice, transfer };

        {
                const int bytesPerRow = gameWidth * 4;
                auto *dst = static_cast<uint8_t *>(SDL_MapGPUTransferBuffer(gDevice, transfer, false));
                const auto *src = static_cast<const uint8_t *>(frame.pixels);
                if (frame.pitch == bytesPerRow) {
                        std::memcpy(dst, src, static_cast<size_t>(bytesPerRow) * gameHeight);
                } else {
                        for (int y = 0; y < gameHeight; ++y)
                                std::memcpy(dst + static_cast<size_t>(y) * bytesPerRow, src + static_cast<size_t>(y) * frame.pitch, bytesPerRow);
                }
                SDL_UnmapGPUTransferBuffer(gDevice, transfer);

                SDL_GPUCopyPass *copyPass = SDL_BeginGPUCopyPass(cmd);
                SDL_GPUTextureTransferInfo transferSrc {};
                transferSrc.transfer_buffer = transfer;
                transferSrc.offset = 0;
                transferSrc.pixels_per_row = gameWidth;
                transferSrc.rows_per_layer = gameHeight;
                SDL_GPUTextureRegion region {};
                region.texture = gGameTex;
                region.w = gameWidth;
                region.h = gameHeight;
                region.d = 1;
                SDL_UploadToGPUTexture(copyPass, &transferSrc, &region, false);
                SDL_EndGPUCopyPass(copyPass);
        }

        FragUniforms uniforms {};
        uniforms.sourceSize[0] = static_cast<float>(gameWidth);
        uniforms.sourceSize[1] = static_cast<float>(gameHeight);
        uniforms.sourceSize[2] = 1.0F / gameWidth;
        uniforms.sourceSize[3] = 1.0F / gameHeight;

        if (filter == ShaderFilter::HQ2x || filter == ShaderFilter::HQ3x || filter == ShaderFilter::HQ4x) {
                // Pass 1: edge indices.
                SDL_GPUTextureSamplerBinding indexBindings[1] = { { gGameTex, gNearestSampler } };
                bool passFailed = !RenderPass(cmd, gIndexTex, gameWidth, gameHeight, gHqxIndexPipeline, uniforms, indexBindings, 1, nullptr);

                // Pass 2: LUT blend into the scaled target.
                if (!passFailed) {
                        const int lutIndex = baseScale == 2 ? 0 : baseScale == 3 ? 1 : 2;
                        SDL_GPUTextureSamplerBinding blendBindings[3] = {
                                { gIndexTex, gNearestSampler },
                                { gHqxLuts[lutIndex], gNearestSampler },
                                { gGameTex, gNearestSampler },
                        };
                        uniforms.params[0] = static_cast<float>(baseScale);
                        passFailed = !RenderPass(cmd, gScaledTex, gameWidth * passScale, gameHeight * passScale, gHqxBlendPipeline, uniforms, blendBindings, 3, nullptr);
                }
                if (passFailed) {
                        SDL_CancelGPUCommandBuffer(cmd);
                        return;
                }
        } else {
                // ScaleFX pass 0: metrics.
                SDL_GPUTextureSamplerBinding p0Bindings[1] = { { gGameTex, gNearestSampler } };
                bool passFailed = !RenderPass(cmd, gMetricTex, gameWidth, gameHeight, gSfxPipelines[0], uniforms, p0Bindings, 1, nullptr);

                // ScaleFX pass 1: corner strength.
                if (!passFailed) {
                        SDL_GPUTextureSamplerBinding p1Bindings[1] = { { gMetricTex, gNearestSampler } };
                        uniforms.params[0] = kSfxClr;
                        uniforms.params[1] = kSfxSaa;
                        passFailed = !RenderPass(cmd, gStrengthTex, gameWidth, gameHeight, gSfxPipelines[1], uniforms, p1Bindings, 1, nullptr);
                }
                // ScaleFX pass 2: edge flags.
                if (!passFailed) {
                        SDL_GPUTextureSamplerBinding p2Bindings[2] = {
                                { gStrengthTex, gNearestSampler },
                                { gMetricTex, gNearestSampler },
                        };
                        uniforms.params[0] = 0.0F;
                        uniforms.params[1] = 0.0F;
                        passFailed = !RenderPass(cmd, gFlagsTex, gameWidth, gameHeight, gSfxPipelines[2], uniforms, p2Bindings, 2, nullptr);
                }
                // ScaleFX pass 3: subpixel map.
                if (!passFailed) {
                        SDL_GPUTextureSamplerBinding p3Bindings[1] = { { gFlagsTex, gNearestSampler } };
                        uniforms.params[0] = kSfxScn;
                        passFailed = !RenderPass(cmd, gSubpixTex, gameWidth, gameHeight, gSfxPipelines[3], uniforms, p3Bindings, 1, nullptr);
                }
                // ScaleFX pass 4: render at the effective scale.
                if (!passFailed) {
                        SDL_GPUTextureSamplerBinding p4Bindings[2] = {
                                { gSubpixTex, gNearestSampler },
                                { gGameTex, gNearestSampler },
                        };
                        uniforms.params[0] = static_cast<float>(passScale);
                        passFailed = !RenderPass(cmd, gScaledTex, gameWidth * passScale, gameHeight * passScale, gSfxPipelines[4], uniforms, p4Bindings, 2, nullptr);
                }
                if (passFailed) {
                        SDL_CancelGPUCommandBuffer(cmd);
                        return;
                }
        }

        // Blit the scaled image into the swapchain with letterboxing.
        int destWidth = static_cast<int>(std::lround(gameWidth * destScale));
        int destHeight = static_cast<int>(std::lround(gameHeight * destScale));
        // Rounding can overshoot the swapchain by a pixel; clamp so the
        // viewport never exceeds the render target.
        if (destWidth > static_cast<int>(swapchainWidth))
                destWidth = static_cast<int>(swapchainWidth);
        if (destHeight > static_cast<int>(swapchainHeight))
                destHeight = static_cast<int>(swapchainHeight);
        if (destWidth <= 0 || destHeight <= 0) {
                // Window smaller than the scaled image can occupy; skip.
                SDL_CancelGPUCommandBuffer(cmd);
                return;
        }
        SDL_GPUViewport viewport {};
        viewport.x = (swapchainWidth - destWidth) / 2.0F;
        viewport.y = (swapchainHeight - destHeight) / 2.0F;
        viewport.w = destWidth;
        viewport.h = destHeight;
        viewport.min_depth = 0.0F;
        viewport.max_depth = 1.0F;
        SDL_GPUTextureSamplerBinding blitBindings[1] = { { gScaledTex, gLinearSampler } };
        uniforms.params[0] = 0.0F;
        if (!RenderPass(cmd, swapchainTexture, swapchainWidth, swapchainHeight, gBlitPipeline, uniforms, blitBindings, 1, &viewport)) {
                SDL_CancelGPUCommandBuffer(cmd);
                return;
        }

        if (!SDL_SubmitGPUCommandBuffer(cmd))
                LogFailHard("SDL_SubmitGPUCommandBuffer failed");
}

namespace {

std::string gOsdText;

const char *FilterDisplayName(ShaderFilter filter)
{
        switch (filter) {
        case ShaderFilter::HQ2x:
                return "HQ2x";
        case ShaderFilter::HQ3x:
                return "HQ3x";
        case ShaderFilter::HQ4x:
                return "HQ4x";
        case ShaderFilter::ScaleFX2x:
                return "ScaleFX 2x";
        case ShaderFilter::ScaleFX3x:
                return "ScaleFX 3x";
        case ShaderFilter::ScaleFX4x:
                return "ScaleFX 4x";
        case ShaderFilter::None:
                break;
        }
        return "Off";
}

std::string PrettyBackendName(std::string_view driver)
{
        if (driver == "vulkan")
                return "Vulkan";
        if (driver == "direct3d12")
                return "Direct3D 12";
        if (driver == "metal")
                return "Metal";
        return std::string(driver);
}

} // namespace

void ShowFilterOsd()
{
        const int value = static_cast<int>(*GetOptions().Graphics.shaderFilter);
        std::string text;
        if (value == static_cast<int>(ShaderFilter::None)) {
                text = "Shader Filter: Off";
        } else if (!IsActive()) {
                text = StrCat("Shader Filter: ", FilterDisplayName(static_cast<ShaderFilter>(value)), " (unavailable)");
        } else {
                text = StrCat("Shader Filter: ", FilterDisplayName(static_cast<ShaderFilter>(value)),
                    " (", PrettyBackendName(SDL_GetGPUDeviceDriver(gDevice)), ")");
        }
        if (text != gOsdText) {
                gOsdText = std::move(text);
                LogFileLine(StrCat("osd: ", gOsdText));
        }
}

std::string_view FilterOsdText()
{
        return gOsdText;
}

void DrawFilterOsd(const Surface &out)
{
        const std::string_view text = FilterOsdText();
        if (text.empty())
                return;
        constexpr int Margin = 8;
        const int textWidth = GetLineWidth(text);
        const int textHeight = GetLineHeight(text, GameFont12);
        const Point position { out.w() - Margin - textWidth, out.h() - Margin - textHeight };
        DrawString(out, text, position, { .flags = UiFlags::ColorWhite });
}

} // namespace gpu_scaler
} // namespace devilution
#endif // !USE_SDL1
#endif // USE_GPU_SCALER
