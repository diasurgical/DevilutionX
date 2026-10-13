#include "engine/render/gpu/gpu_d3dcompile.hpp"

#ifdef _WIN32

#include <algorithm>
#include <csetjmp>
#include <cstring>
#include <string>

#include <windows.h>

#include "engine/render/gpu/gpu_scaler.hpp"
#include "utils/log.hpp"
#include "utils/str_cat.hpp"

namespace devilution {
namespace gpu_d3dcompile {
namespace {

// Diagnostic logging goes through the GPU scaler's public LogPhase hook so
// every message lands in the same gpu-scaler.log timeline.

void LogFileLine(std::string_view line)
{
	gpu_scaler::LogPhase(line);
}

// --- Guarded D3DCompile ------------------------------------------------------
// A fault inside d3dcompiler (a wild jump into a non-executable page has been
// observed in the wild on D3D12, killing the process before the fallback could
// run) must not take the game down: while the guard is armed on this thread,
// the vectored exception handler below converts the fault into a "compile
// failed" result via longjmp, and the presentation path either retries on the
// Vulkan backend or reverts to the regular renderer for the session. The
// guarded span contains no C++ objects with destructors, so skipping
// unwinding is safe; x64 exception dispatch is table-based, so abandoning the
// faulting frames leaves no stale registration behind.
thread_local bool tCompileGuardArmed = false;
thread_local DWORD tCompileGuardThread = 0;
static jmp_buf tCompileJmpBuf;

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
	if (tCompileGuardArmed && info != nullptr && info->ExceptionRecord != nullptr
	    && info->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION
	    && GetCurrentThreadId() == tCompileGuardThread) {
		tCompileGuardArmed = false;
		LogFileLine("ERROR: fault inside D3DCompile; recovered by the compile guard");
		longjmp(tCompileJmpBuf, 1);
	}
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

// Lossy ASCII rendering of a Windows path, for the diagnostic log only.
std::string NarrowPath(const wchar_t *wide)
{
	std::string narrow;
	for (const wchar_t *c = wide; *c != L'\0'; ++c)
		narrow.push_back(*c >= 32 && *c < 127 ? static_cast<char>(*c) : '?');
	return narrow;
}

D3DCompileFn LoadD3DCompile()
{
	static D3DCompileFn fn = nullptr;
	if (fn != nullptr)
		return fn;
	// Load by explicit path from the system directory. The default search
	// order would pick up a same-named d3dcompiler DLL sitting in the game
	// folder first, and stale copies shipped by old games are the leading
	// suspect for wild jumps inside D3DCompile.
	wchar_t sysDir[MAX_PATH];
	const UINT sysLen = GetSystemDirectoryW(sysDir, MAX_PATH);
	if (sysLen == 0 || sysLen >= MAX_PATH - 32) {
		LogError("GPU scaler: could not resolve the system directory");
		return nullptr;
	}
	HMODULE mod = nullptr;
	for (const wchar_t *name : { L"\\d3dcompiler_47.dll", L"\\d3dcompiler_43.dll" }) {
		std::wstring path(sysDir);
		path += name;
		mod = LoadLibraryW(path.c_str());
		if (mod != nullptr)
			break;
	}
	if (mod == nullptr)
		return nullptr;
	// Log exactly which d3dcompiler DLL ended up in the process: Windows
	// matches LoadLibrary by base file name, so if any component loaded a
	// same-named DLL from the game folder earlier, the full system path above
	// still resolves to that (possibly broken) module.
	wchar_t modPath[MAX_PATH] = {};
	GetModuleFileNameW(mod, modPath, MAX_PATH);
	const std::string loaded = NarrowPath(modPath);
	const std::string systemDir = NarrowPath(sysDir);
	if (loaded.rfind(systemDir, 0) == 0) {
		LogFileLine(StrCat("d3dcompiler: ", loaded));
	} else {
		LogFileLine(StrCat("WARNING: d3dcompiler loaded from outside the system directory: ", loaded,
		    " (a same-named DLL next to the game was loaded earlier by name; delete the local copy)"));
	}
	fn = reinterpret_cast<D3DCompileFn>(reinterpret_cast<void *>(GetProcAddress(mod, "D3DCompile")));
	if (fn == nullptr)
		LogError("GPU scaler: found d3dcompiler dll but no D3DCompile export");
	return fn;
}

// Bare invocation wrapper for D3DCompile: must not contain C++ objects with
// destructors, because a fault mid-call resumes here via longjmp from the
// vectored exception handler.
long InvokeD3DCompile(D3DCompileFn d3dCompile, const char *source, const char *target,
    FakeD3DBlob **code, FakeD3DBlob **errors)
{
	volatile long hr = static_cast<long>(0x8000FFFFu); // E_UNEXPECTED
	if (setjmp(tCompileJmpBuf) != 0) {
		// Faulted inside d3dcompiler; the handler has disarmed the guard.
		return hr;
	}
	tCompileGuardArmed = true;
	tCompileGuardThread = GetCurrentThreadId();
	hr = d3dCompile(source, std::strlen(source), nullptr, nullptr, nullptr,
	    "main", target, 0, 0, code, errors);
	tCompileGuardArmed = false;
	return hr;
}

// The same containment for the returned blob objects as for the D3DCompile
// call itself: a broken d3dcompiler can hand back a blob whose vtable is
// garbage, and the wild jumps observed in the wild happened on these virtual
// calls right after D3DCompile returned, not inside it.
const void *GuardedBlobPointer(FakeD3DBlob *blob)
{
	const volatile void *result = nullptr;
	if (setjmp(tCompileJmpBuf) != 0)
		return nullptr;
	tCompileGuardArmed = true;
	tCompileGuardThread = GetCurrentThreadId();
	result = blob->GetBufferPointer();
	tCompileGuardArmed = false;
	return const_cast<const void *>(result);
}

size_t GuardedBlobSize(FakeD3DBlob *blob)
{
	volatile size_t result = 0;
	if (setjmp(tCompileJmpBuf) != 0)
		return 0;
	tCompileGuardArmed = true;
	tCompileGuardThread = GetCurrentThreadId();
	result = blob->GetBufferSize();
	tCompileGuardArmed = false;
	return result;
}

void GuardedBlobRelease(FakeD3DBlob *blob)
{
	if (blob == nullptr)
		return;
	if (setjmp(tCompileJmpBuf) != 0)
		return;
	tCompileGuardArmed = true;
	tCompileGuardThread = GetCurrentThreadId();
	blob->Release();
	tCompileGuardArmed = false;
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
	const long hr = InvokeD3DCompile(d3dCompile, source, target, &code, &errors);
	if (hr == static_cast<long>(0x8000FFFFu)) {
		// The compile guard caught a fault inside d3dcompiler.
		LogError("GPU scaler: D3DCompile faulted and was contained; treating '{}' as a compile failure", target);
		LogFileLine(StrCat("ERROR: D3DCompile faulted (", target, "); contained by the compile guard"));
		return blob;
	}
	if (hr < 0 || code == nullptr) {
		std::string msg;
		if (errors != nullptr) {
			const char *errorBytes = static_cast<const char *>(GuardedBlobPointer(errors));
			const size_t errorSize = GuardedBlobSize(errors);
			GuardedBlobRelease(errors);
			if (errorBytes != nullptr && errorSize > 0)
				msg.assign(errorBytes, std::min<size_t>(errorSize, 400));
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
	const auto *bytes = static_cast<const uint8_t *>(GuardedBlobPointer(code));
	const size_t size = GuardedBlobSize(code);
	GuardedBlobRelease(code);
	if (bytes == nullptr || size == 0) {
		LogFileLine(StrCat("ERROR: d3dcompiler returned an unusable shader blob for '", target, "'"));
		return blob;
	}
	blob.assign(bytes, bytes + size);
	return blob;
}

} // namespace

void PreloadSystemCompiler()
{
	wchar_t sysDir[MAX_PATH];
	const UINT sysLen = GetSystemDirectoryW(sysDir, MAX_PATH);
	if (sysLen == 0 || sysLen >= MAX_PATH - 32)
		return;
	for (const wchar_t *name : { L"\\d3dcompiler_47.dll", L"\\d3dcompiler_43.dll" }) {
		std::wstring path(sysDir);
		path += name;
		if (LoadLibraryW(path.c_str()) != nullptr)
			return; // Keep it loaded for the whole process.
	}
}

void InstallCrashGuard()
{
	static bool installed = false;
	if (!installed) {
		installed = true;
		AddVectoredExceptionHandler(/*First=*/1, &ScalerCrashHandler);
	}
}

std::vector<uint8_t> Compile(const char *source, const char *target)
{
	return CompileHLSL(source, target);
}

} // namespace gpu_d3dcompile
} // namespace devilution

#endif // _WIN32
