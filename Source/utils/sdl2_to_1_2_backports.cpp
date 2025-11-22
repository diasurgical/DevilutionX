#include "utils/sdl2_to_1_2_backports.h"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <string_view>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX 1
#ifndef DEVILUTIONX_WINDOWS_NO_WCHAR
#define UNICODE 1
#include <shlobj.h>
#endif
#include <windows.h>
#endif

#include "utils/console.h"
#include "utils/str_cat.hpp"

#define DEFAULT_PRIORITY SDL_LOG_PRIORITY_CRITICAL
#define DEFAULT_ASSERT_PRIORITY SDL_LOG_PRIORITY_WARN
#define DEFAULT_APPLICATION_PRIORITY SDL_LOG_PRIORITY_INFO
#define DEFAULT_TEST_PRIORITY SDL_LOG_PRIORITY_VERBOSE

namespace {

// We use the same names of these structs as the SDL2 implementation:
// NOLINTNEXTLINE(readability-identifier-naming)
struct SDL_LogLevel {
	int category;
	SDL_LogPriority priority;
	SDL_LogLevel *next;
};

SDL_LogLevel *SDL_loglevels;                                             // NOLINT(readability-identifier-naming)
SDL_LogPriority SDL_default_priority = DEFAULT_PRIORITY;                 // NOLINT(readability-identifier-naming)
SDL_LogPriority SDL_assert_priority = DEFAULT_ASSERT_PRIORITY;           // NOLINT(readability-identifier-naming)
SDL_LogPriority SDL_application_priority = DEFAULT_APPLICATION_PRIORITY; // NOLINT(readability-identifier-naming)
SDL_LogPriority SDL_test_priority = DEFAULT_TEST_PRIORITY;               // NOLINT(readability-identifier-naming)

// NOLINTNEXTLINE(readability-identifier-naming)
const char *const SDL_priority_prefixes[SDL_NUM_LOG_PRIORITIES] = {
	nullptr,
	"VERBOSE",
	"DEBUG",
	"INFO",
	"WARN",
	"ERROR",
	"CRITICAL"
};

} // namespace

void SDL_LogSetAllPriority(SDL_LogPriority priority)
{
	for (SDL_LogLevel *entry = SDL_loglevels; entry != nullptr; entry = entry->next) {
		entry->priority = priority;
	}
	SDL_default_priority = priority;
	SDL_assert_priority = priority;
	SDL_application_priority = priority;
}

void SDL_LogSetPriority(int category, SDL_LogPriority priority)
{
	SDL_LogLevel *entry;
	for (entry = SDL_loglevels; entry != nullptr; entry = entry->next) {
		if (entry->category == category) {
			entry->priority = priority;
			return;
		}
	}

	entry = static_cast<SDL_LogLevel *>(SDL_malloc(sizeof(*entry)));
	if (entry != nullptr) {
		entry->category = category;
		entry->priority = priority;
		entry->next = SDL_loglevels;
		SDL_loglevels = entry;
	}
}

SDL_LogPriority SDL_LogGetPriority(int category)
{
	for (SDL_LogLevel *entry = SDL_loglevels; entry != nullptr; entry = entry->next) {
		if (entry->category == category) {
			return entry->priority;
		}
	}

	switch (category) {
	case SDL_LOG_CATEGORY_TEST:
		return SDL_test_priority;
	case SDL_LOG_CATEGORY_APPLICATION:
		return SDL_application_priority;
	case SDL_LOG_CATEGORY_ASSERT:
		return SDL_assert_priority;
	default:
		return SDL_default_priority;
	}
}

void SDL_Log(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	SDL_LogMessageV(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO, fmt, ap);
	va_end(ap);
}

void SDL_LogVerbose(int category, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	SDL_LogMessageV(category, SDL_LOG_PRIORITY_VERBOSE, fmt, ap);
	va_end(ap);
}

void SDL_LogDebug(int category, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	SDL_LogMessageV(category, SDL_LOG_PRIORITY_DEBUG, fmt, ap);
	va_end(ap);
}

void SDL_LogInfo(int category, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	SDL_LogMessageV(category, SDL_LOG_PRIORITY_INFO, fmt, ap);
	va_end(ap);
}

void SDL_LogWarn(int category, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	SDL_LogMessageV(category, SDL_LOG_PRIORITY_WARN, fmt, ap);
	va_end(ap);
}

void SDL_LogError(int category, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	SDL_LogMessageV(category, SDL_LOG_PRIORITY_ERROR, fmt, ap);
	va_end(ap);
}

void SDL_LogCritical(int category, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	SDL_LogMessageV(category, SDL_LOG_PRIORITY_CRITICAL, fmt, ap);
	va_end(ap);
}

void SDL_LogMessage(int category, SDL_LogPriority priority, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	SDL_LogMessageV(category, priority, fmt, ap);
	va_end(ap);
}

void SDL_LogMessageV(int category, SDL_LogPriority priority, const char *fmt, va_list ap)
{
	if (static_cast<int>(priority) < 0 || priority >= SDL_NUM_LOG_PRIORITIES || SDL_LogGetPriority(category) > priority)
		return;

	::devilution::printfInConsole("%s: ", SDL_priority_prefixes[priority]);
	::devilution::vprintfInConsole(fmt, ap);
	::devilution::printNewlineInConsole();
}

namespace {

#define DEFINE_COPY_ROW(name, type)                       \
	void name(type *src, int src_w, type *dst, int dst_w) \
	{                                                     \
		type pixel = 0;                                   \
                                                          \
		int pos = 0x10000;                                \
		int inc = (src_w << 16) / dst_w;                  \
		for (int i = dst_w; i > 0; i--) {                 \
			while (pos >= 0x10000L) {                     \
				pixel = *src++;                           \
				pos -= 0x10000L;                          \
			}                                             \
			*dst++ = pixel;                               \
			pos += inc;                                   \
		}                                                 \
	}
DEFINE_COPY_ROW(copy_row1, Uint8)
DEFINE_COPY_ROW(copy_row2, Uint16)
DEFINE_COPY_ROW(copy_row4, Uint32)

void copy_row3(Uint8 *src, int src_w, Uint8 *dst, int dst_w)
{
	Uint8 pixel[3] = { 0, 0, 0 };

	int pos = 0x10000;
	int inc = (src_w << 16) / dst_w;
	for (int i = dst_w; i > 0; --i) {
		while (pos >= 0x10000L) {
			pixel[0] = *src++;
			pixel[1] = *src++;
			pixel[2] = *src++;
			pos -= 0x10000L;
		}
		*dst++ = pixel[0];
		*dst++ = pixel[1];
		*dst++ = pixel[2];
		pos += inc;
	}
}

} // namespace

int SDL_SoftStretch(SDL_Surface *src, const SDL_Rect *srcrect,
    SDL_Surface *dst, const SDL_Rect *dstrect)
{
	// All the ASM support has been removed, as the platforms that the ASM
	// implementation exists for support SDL2 anyway.
	int src_locked;
	int dst_locked;
	int pos, inc;
	int dst_maxrow;
	int src_row, dst_row;
	Uint8 *srcp = NULL;
	Uint8 *dstp;
	SDL_Rect full_src;
	SDL_Rect full_dst;
	const int bpp = dst->format->BytesPerPixel;

	if (!SDLBackport_PixelFormatFormatEq(src->format, dst->format)) {
		SDL_SetError("Only works with same format surfaces");
		return -1;
	}

	/* Verify the blit rectangles */
	if (srcrect) {
		if ((srcrect->x < 0) || (srcrect->y < 0) || ((srcrect->x + srcrect->w) > src->w) || ((srcrect->y + srcrect->h) > src->h)) {
			SDL_SetError("Invalid source blit rectangle");
			return -1;
		}
	} else {
		full_src.x = 0;
		full_src.y = 0;
		full_src.w = src->w;
		full_src.h = src->h;
		srcrect = &full_src;
	}
	if (dstrect) {
		if ((dstrect->x < 0) || (dstrect->y < 0) || ((dstrect->x + dstrect->w) > dst->w) || ((dstrect->y + dstrect->h) > dst->h)) {
			SDL_SetError("Invalid destination blit rectangle");
			return -1;
		}
	} else {
		full_dst.x = 0;
		full_dst.y = 0;
		full_dst.w = dst->w;
		full_dst.h = dst->h;
		dstrect = &full_dst;
	}

	/* Lock the destination if it's in hardware */
	dst_locked = 0;
	if (SDL_MUSTLOCK(dst)) {
		if (SDL_LockSurface(dst) < 0) {
			SDL_SetError("Unable to lock destination surface");
			return -1;
		}
		dst_locked = 1;
	}
	/* Lock the source if it's in hardware */
	src_locked = 0;
	if (SDL_MUSTLOCK(src)) {
		if (SDL_LockSurface(src) < 0) {
			if (dst_locked) {
				SDL_UnlockSurface(dst);
			}
			SDL_SetError("Unable to lock source surface");
			return -1;
		}
		src_locked = 1;
	}

	/* Set up the data... */
	pos = 0x10000;
	inc = (srcrect->h << 16) / dstrect->h;
	src_row = srcrect->y;
	dst_row = dstrect->y;

	/* Perform the stretch blit */
	for (dst_maxrow = dst_row + dstrect->h; dst_row < dst_maxrow; ++dst_row) {
		dstp = (Uint8 *)dst->pixels + (dst_row * dst->pitch)
		    + (dstrect->x * bpp);
		while (pos >= 0x10000L) {
			srcp = (Uint8 *)src->pixels + (src_row * src->pitch)
			    + (srcrect->x * bpp);
			++src_row;
			pos -= 0x10000L;
		}
		switch (bpp) {
		case 1:
			copy_row1(srcp, srcrect->w, dstp, dstrect->w);
			break;
		case 2:
			copy_row2((Uint16 *)srcp, srcrect->w,
			    (Uint16 *)dstp, dstrect->w);
			break;
		case 3:
			copy_row3(srcp, srcrect->w, dstp, dstrect->w);
			break;
		case 4:
			copy_row4((Uint32 *)srcp, srcrect->w,
			    (Uint32 *)dstp, dstrect->w);
			break;
		}
		pos += inc;
	}

	/* We need to unlock the surfaces if they're locked */
	if (dst_locked) {
		SDL_UnlockSurface(dst);
	}
	if (src_locked) {
		SDL_UnlockSurface(src);
	}
	return 0;
}

int SDL_BlitScaled(SDL_Surface *src, SDL_Rect *srcrect,
    SDL_Surface *dst, SDL_Rect *dstrect)
{
	if (!SDLBackport_PixelFormatFormatEq(src->format, dst->format) || SDLBackport_IsPixelFormatIndexed(src->format)) {
		return SDL_BlitSurface(src, srcrect, dst, dstrect);
	}

	float src_x0, src_y0, src_x1, src_y1;
	float dst_x0, dst_y0, dst_x1, dst_y1;
	SDL_Rect final_src, final_dst;
	float scaling_w, scaling_h;
	int src_w, src_h;
	int dst_w, dst_h;

	/* Make sure the surfaces aren't locked */
	if (!src || !dst) {
		SDL_SetError("SDL_UpperBlitScaled: passed a NULL surface");
		return -1;
	}
	if (src->locked || dst->locked) {
		SDL_SetError("Surfaces must not be locked during blit");
		return -1;
	}

	if (NULL == srcrect) {
		src_w = src->w;
		src_h = src->h;
	} else {
		src_w = srcrect->w;
		src_h = srcrect->h;
	}

	if (NULL == dstrect) {
		dst_w = dst->w;
		dst_h = dst->h;
	} else {
		dst_w = dstrect->w;
		dst_h = dstrect->h;
	}

	if (dst_w == src_w && dst_h == src_h) {
		/* No scaling, defer to regular blit */
		return SDL_BlitSurface(src, srcrect, dst, dstrect);
	}

	scaling_w = (float)dst_w / src_w;
	scaling_h = (float)dst_h / src_h;

	if (NULL == dstrect) {
		dst_x0 = 0;
		dst_y0 = 0;
		dst_x1 = static_cast<float>(dst_w - 1);
		dst_y1 = static_cast<float>(dst_h - 1);
	} else {
		dst_x0 = dstrect->x;
		dst_y0 = dstrect->y;
		dst_x1 = dst_x0 + dst_w - 1;
		dst_y1 = dst_y0 + dst_h - 1;
	}

	if (NULL == srcrect) {
		src_x0 = 0;
		src_y0 = 0;
		src_x1 = static_cast<float>(src_w - 1);
		src_y1 = static_cast<float>(src_h - 1);
	} else {
		src_x0 = srcrect->x;
		src_y0 = srcrect->y;
		src_x1 = src_x0 + src_w - 1;
		src_y1 = src_y0 + src_h - 1;

		/* Clip source rectangle to the source surface */

		if (src_x0 < 0) {
			dst_x0 -= src_x0 * scaling_w;
			src_x0 = 0;
		}

		if (src_x1 >= src->w) {
			dst_x1 -= (src_x1 - src->w + 1) * scaling_w;
			src_x1 = static_cast<float>(src->w - 1);
		}

		if (src_y0 < 0) {
			dst_y0 -= src_y0 * scaling_h;
			src_y0 = 0;
		}

		if (src_y1 >= src->h) {
			dst_y1 -= (src_y1 - src->h + 1) * scaling_h;
			src_y1 = static_cast<float>(src->h - 1);
		}
	}

	/* Clip destination rectangle to the clip rectangle */

	/* Translate to clip space for easier calculations */
	dst_x0 -= dst->clip_rect.x;
	dst_x1 -= dst->clip_rect.x;
	dst_y0 -= dst->clip_rect.y;
	dst_y1 -= dst->clip_rect.y;

	if (dst_x0 < 0) {
		src_x0 -= dst_x0 / scaling_w;
		dst_x0 = 0;
	}

	if (dst_x1 >= dst->clip_rect.w) {
		src_x1 -= (dst_x1 - dst->clip_rect.w + 1) / scaling_w;
		dst_x1 = static_cast<float>(dst->clip_rect.w - 1);
	}

	if (dst_y0 < 0) {
		src_y0 -= dst_y0 / scaling_h;
		dst_y0 = 0;
	}

	if (dst_y1 >= dst->clip_rect.h) {
		src_y1 -= (dst_y1 - dst->clip_rect.h + 1) / scaling_h;
		dst_y1 = static_cast<float>(dst->clip_rect.h - 1);
	}

	/* Translate back to surface coordinates */
	dst_x0 += dst->clip_rect.x;
	dst_x1 += dst->clip_rect.x;
	dst_y0 += dst->clip_rect.y;
	dst_y1 += dst->clip_rect.y;

	final_src.x = static_cast<Sint16>(SDL_floor(src_x0 + 0.5));
	final_src.y = static_cast<Sint16>(SDL_floor(src_y0 + 0.5));
	src_w = std::max(static_cast<int>(SDL_floor(src_x1 + 1 + 0.5)) - static_cast<int>(SDL_floor(src_x0 + 0.5)), 0);
	src_h = std::max(static_cast<int>(SDL_floor(src_y1 + 1 + 0.5)) - static_cast<int>(SDL_floor(src_y0 + 0.5)), 0);

	final_src.w = static_cast<Uint16>(src_w);
	final_src.h = static_cast<Uint16>(src_h);

	final_dst.x = static_cast<Sint16>(SDL_floor(dst_x0 + 0.5));
	final_dst.y = static_cast<Sint16>(SDL_floor(dst_y0 + 0.5));
	dst_w = std::max(static_cast<int>(SDL_floor(dst_x1 - dst_x0 + 1.5)), 0);
	dst_h = std::max(static_cast<int>(SDL_floor(dst_y1 - dst_y0 + 1.5)), 0);

	final_dst.w = static_cast<Uint16>(dst_w);
	final_dst.h = static_cast<Uint16>(dst_h);

	if (dstrect)
		*dstrect = final_dst;

	if (final_dst.w == 0 || final_dst.h == 0 || final_src.w == 0 || final_src.h == 0) {
		/* No-op. */
		return 0;
	}

	return SDL_SoftStretch(src, &final_src, dst, &final_dst);
}

// = Filesystem

Sint64 SDL_RWsize(SDL_RWops *context)
{
	const int current = SDL_RWtell(context);
	if (current == -1)
		return -1;

	const int begin = SDL_RWseek(context, 0, RW_SEEK_SET);
	if (begin == -1)
		return -1;

	const int end = SDL_RWseek(context, 0, RW_SEEK_END);
	if (end == -1)
		return -1;

	if (SDL_RWseek(context, current, RW_SEEK_SET) == -1)
		return -1;

	return end - begin;
}

#if defined(_WIN32) && !defined(DEVILUTIONX_WINDOWS_NO_WCHAR)

namespace {

// From sdl2-2.0.9/src/core/windows/SDL_windows.h
#define WIN_StringToUTF8(S) SDL_iconv_string("UTF-8", "UTF-16LE", (char *)(S), (wcslen(S) + 1) * sizeof(WCHAR))
#define WIN_UTF8ToString(S) (WCHAR *)SDL_iconv_string("UTF-16LE", "UTF-8", (char *)(S), SDL_strlen(S) + 1)

/* Sets an error message based on an HRESULT */
int WIN_SetErrorFromHRESULT(const char *prefix, HRESULT hr)
{
	// From sdl2-2.0.9/src/core/windows/SDL_windows.c

	TCHAR buffer[1024];
	char *message;
	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, hr, 0,
	    buffer, SDL_arraysize(buffer), NULL);
	message = WIN_StringToUTF8(buffer);
	SDL_SetError("%s%s%s", prefix ? prefix : "", prefix ? ": " : "", message);
	SDL_free(message);
	return -1;
}

/* Sets an error message based on GetLastError() */
int WIN_SetError(const char *prefix)
{
	// From sdl2-2.0.9/src/core/windows/SDL_windows.c

	return WIN_SetErrorFromHRESULT(prefix, GetLastError());
}

} // namespace

extern "C" char *SDL_GetBasePath(void)
{
	// From sdl2-2.0.9/src/filesystem/windows/SDL_sysfilesystem.c

	typedef DWORD(WINAPI * GetModuleFileNameExW_t)(HANDLE, HMODULE, LPWSTR, DWORD);
	GetModuleFileNameExW_t pGetModuleFileNameExW;
	DWORD buflen = 128;
	WCHAR *path = NULL;
	HMODULE psapi = LoadLibrary(L"psapi.dll");
	char *retval = NULL;
	DWORD len = 0;
	int i;

	if (!psapi) {
		WIN_SetError("Couldn't load psapi.dll");
		return NULL;
	}

	pGetModuleFileNameExW = (GetModuleFileNameExW_t)GetProcAddress(psapi, "GetModuleFileNameExW");
	if (!pGetModuleFileNameExW) {
		WIN_SetError("Couldn't find GetModuleFileNameExW");
		FreeLibrary(psapi);
		return NULL;
	}

	while (SDL_TRUE) {
		void *ptr = SDL_realloc(path, buflen * sizeof(WCHAR));
		if (!ptr) {
			SDL_free(path);
			FreeLibrary(psapi);
			SDL_OutOfMemory();
			return NULL;
		}

		path = (WCHAR *)ptr;

		len = pGetModuleFileNameExW(GetCurrentProcess(), NULL, path, buflen);
		if (len != buflen) {
			break;
		}

		/* buffer too small? Try again. */
		buflen *= 2;
	}

	FreeLibrary(psapi);

	if (len == 0) {
		SDL_free(path);
		WIN_SetError("Couldn't locate our .exe");
		return NULL;
	}

	for (i = len - 1; i > 0; i--) {
		if (path[i] == '\\') {
			break;
		}
	}

	path[i + 1] = '\0'; /* chop off filename. */

	retval = WIN_StringToUTF8(path);
	SDL_free(path);

	return retval;
}

extern "C" char *SDL_GetPrefPath(const char *org, const char *app)
{
	// From sdl2-2.0.9/src/filesystem/windows/SDL_sysfilesystem.c

	/*
	 * Vista and later has a new API for this, but SHGetFolderPath works there,
	 *  and apparently just wraps the new API. This is the new way to do it:
	 *
	 *     SHGetKnownFolderPath(FOLDERID_RoamingAppData, KF_FLAG_CREATE,
	 *                          NULL, &wszPath);
	 */

	WCHAR path[MAX_PATH];
	char *retval = NULL;
	WCHAR *worg = NULL;
	WCHAR *wapp = NULL;
	size_t new_wpath_len = 0;
	BOOL api_result = FALSE;

	if (!app) {
		SDL_InvalidParamError("app");
		return NULL;
	}
	if (!org) {
		org = "";
	}

	if (!SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_APPDATA | CSIDL_FLAG_CREATE, NULL, 0, path))) {
		WIN_SetError("Couldn't locate our prefpath");
		return NULL;
	}

	worg = WIN_UTF8ToString(org);
	if (worg == NULL) {
		SDL_OutOfMemory();
		return NULL;
	}

	wapp = WIN_UTF8ToString(app);
	if (wapp == NULL) {
		SDL_free(worg);
		SDL_OutOfMemory();
		return NULL;
	}

	new_wpath_len = lstrlenW(worg) + lstrlenW(wapp) + lstrlenW(path) + 3;

	if ((new_wpath_len + 1) > MAX_PATH) {
		SDL_free(worg);
		SDL_free(wapp);
		WIN_SetError("Path too long.");
		return NULL;
	}

	if (*worg) {
		lstrcatW(path, L"\\");
		lstrcatW(path, worg);
	}
	SDL_free(worg);

	api_result = CreateDirectoryW(path, NULL);
	if (api_result == FALSE) {
		if (GetLastError() != ERROR_ALREADY_EXISTS) {
			SDL_free(wapp);
			WIN_SetError("Couldn't create a prefpath.");
			return NULL;
		}
	}

	lstrcatW(path, L"\\");
	lstrcatW(path, wapp);
	SDL_free(wapp);

	api_result = CreateDirectoryW(path, NULL);
	if (api_result == FALSE) {
		if (GetLastError() != ERROR_ALREADY_EXISTS) {
			WIN_SetError("Couldn't create a prefpath.");
			return NULL;
		}
	}

	lstrcatW(path, L"\\");

	retval = WIN_StringToUTF8(path);

	return retval;
}

// For Apple, definitions are in Source/platform/macos_sdl1/SDL_filesystem.m
#elif !defined(__APPLE__)

namespace {
#if !defined(__QNXNTO__) && !defined(__amigaos__) && !(defined(WINVER) && WINVER <= 0x0500 && (!defined(_WIN32_WINNT) || _WIN32_WINNT == 0))
char *readSymLink(const char *path)
{
	// From sdl2-2.0.9/src/filesystem/unix/SDL_sysfilesystem.c
	char *retval = NULL;
	ssize_t len = 64;
	ssize_t rc = -1;

	while (1) {
		char *ptr = (char *)SDL_realloc(retval, (size_t)len);
		if (ptr == NULL) {
			SDL_OutOfMemory();
			break;
		}

		retval = ptr;

		rc = readlink(path, retval, len);
		if (rc == -1) {
			break; /* not a symlink, i/o error, etc. */
		} else if (rc < len) {
			retval[rc] = '\0'; /* readlink doesn't null-terminate. */
			return retval;     /* we're good to go. */
		}

		len *= 2; /* grow buffer, try again. */
	}

	SDL_free(retval);
	return NULL;
}
#endif
} // namespace

extern "C" char *SDL_GetBasePath()
{
	// From sdl2-2.0.9/src/filesystem/unix/SDL_sysfilesystem.c

	char *retval = NULL;

#if defined(WINVER) && WINVER <= 0x0500 && (!defined(_WIN32_WINNT) || _WIN32_WINNT == 0)
	TCHAR buffer[MAX_PATH] = { 0 };
	GetModuleFileName(NULL, buffer, MAX_PATH);
	size_t len = std::string_view(buffer).size();
	while (len > 0) {
		if (buffer[len - 1] == '\\') {
			break;
		}
		--len;
	}
	buffer[len] = '\0';
	retval = static_cast<char *>(SDL_malloc(len + 1));
	if (!retval) {
		SDL_OutOfMemory();
		return NULL;
	}
	SDL_memcpy(retval, buffer, len + 1);
#elif defined(__FREEBSD__)
	char fullpath[PATH_MAX];
	size_t buflen = sizeof(fullpath);
	const int mib[] = { CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1 };
	if (sysctl(mib, SDL_arraysize(mib), fullpath, &buflen, NULL, 0) != -1) {
		retval = SDL_strdup(fullpath);
		if (!retval) {
			SDL_OutOfMemory();
			return NULL;
		}
	}
#elif defined(__OPENBSD__)
	char **retvalargs;
	size_t len;
	const int mib[] = { CTL_KERN, KERN_PROC_ARGS, getpid(), KERN_PROC_ARGV };
	if (sysctl(mib, 4, NULL, &len, NULL, 0) != -1) {
		retvalargs = SDL_malloc(len);
		if (!retvalargs) {
			SDL_OutOfMemory();
			return NULL;
		}
		sysctl(mib, 4, retvalargs, &len, NULL, 0);
		retval = SDL_malloc(PATH_MAX + 1);
		if (retval)
			realpath(retvalargs[0], retval);

		SDL_free(retvalargs);
	}
#elif defined(__SOLARIS__)
	const char *path = getexecname();
	if ((path != NULL) && (path[0] == '/')) { /* must be absolute path... */
		retval = SDL_strdup(path);
		if (!retval) {
			SDL_OutOfMemory();
			return NULL;
		}
	}
#elif defined(__3DS__)
	retval = SDL_strdup("file:sdmc:/3ds/devilutionx/");
#elif defined(__amigaos__)
	retval = SDL_strdup("PROGDIR:");
#else

	/* is a Linux-style /proc filesystem available? */
	if (!retval && (access("/proc", F_OK) == 0)) {
		/* !!! FIXME: after 2.0.6 ships, let's delete this code and just
		              use the /proc/%llu version. There's no reason to have
		              two copies of this plus all the #ifdefs. --ryan. */
#if defined(__FREEBSD__)
		retval = readSymLink("/proc/curproc/file");
#elif defined(__NETBSD__)
		retval = readSymLink("/proc/curproc/exe");
#elif defined(__QNXNTO__)
		retval = SDL_LoadFile("/proc/self/exefile", NULL);
#else
		retval = readSymLink("/proc/self/exe"); /* linux. */
		if (retval == NULL) {
			/* older kernels don't have /proc/self ... try PID version... */
			char path[64];
			const int rc = (int)SDL_snprintf(path, sizeof(path),
			    "/proc/%llu/exe",
			    (unsigned long long)getpid());
			if ((rc > 0) && (static_cast<std::size_t>(rc) < sizeof(path))) {
				retval = readSymLink(path);
			}
		}
#endif
	}

	/* If we had access to argv[0] here, we could check it for a path,
	    or troll through $PATH looking for it, too. */

	if (retval != NULL) { /* chop off filename. */
		char *ptr = SDL_strrchr(retval, '/');
		if (ptr != NULL) {
			*(ptr + 1) = '\0';
		} else { /* shouldn't happen, but just in case... */
			SDL_free(retval);
			retval = NULL;
		}
	}

	if (retval != NULL) {
		/* try to shrink buffer... */
		char *ptr = (char *)SDL_realloc(retval, strlen(retval) + 1);
		if (ptr != NULL)
			retval = ptr; /* oh well if it failed. */
	}
#endif
	return retval;
}

extern "C" char *SDL_GetPrefPath(const char *org, const char *app)
{
	// From sdl2-2.0.9/src/filesystem/unix/SDL_sysfilesystem.c
	/*
	 * We use XDG's base directory spec, even if you're not on Linux.
	 *  This isn't strictly correct, but the results are relatively sane
	 *  in any case.
	 *
	 * https://standards.freedesktop.org/basedir-spec/basedir-spec-latest.html
	 */
#if (defined(WINVER) && WINVER <= 0x0500 && (!defined(_WIN32_WINNT) || _WIN32_WINNT == 0)) || defined(__DJGPP__)
	// On Windows9x there is no such thing as PrefPath. Simply use the current directory.
	char *result = (char *)SDL_malloc(1);
	*result = '\0';
	return result;
#else
	const char *envr = SDL_getenv("XDG_DATA_HOME");
	const char *append;
	char *retval = NULL;
	char *ptr = NULL;
	size_t len = 0;

#if defined(__3DS__)
	retval = SDL_strdup("sdmc:/3ds/devilutionx/");
	return retval;
#elif defined(__amigaos__)
	retval = SDL_strdup("PROGDIR:");
	return retval;
#endif

	if (!app) {
		SDL_InvalidParamError("app");
		return NULL;
	}
	if (!org) {
		org = "";
	}

	if (!envr) {
		/* You end up with "$HOME/.local/share/Game Name 2" */
		envr = SDL_getenv("HOME");
		if (!envr) {
			/* we could take heroic measures with /etc/passwd, but oh well. */
			SDL_SetError("neither XDG_DATA_HOME nor HOME environment is set");
			return NULL;
		}
#if defined(__unix__) || defined(__unix)
		append = "/.local/share/";
#else
		append = "/";
#endif
	} else {
		append = "/";
	}

	len = SDL_strlen(envr);
	if (envr[len - 1] == '/')
		append += 1;

	len += SDL_strlen(append) + SDL_strlen(org) + SDL_strlen(app) + 3;
	retval = (char *)SDL_malloc(len);
	if (!retval) {
		SDL_OutOfMemory();
		return NULL;
	}

	if (*org) {
		*devilution::BufCopy(retval, envr, append, org, "/", app) = '\0';
	} else {
		*devilution::BufCopy(retval, envr, append, app) = '\0';
	}

	for (ptr = retval + 1; *ptr; ptr++) {
		if (*ptr == '/') {
			*ptr = '\0';
			if (mkdir(retval, 0700) != 0 && errno != EEXIST)
				goto error;
			*ptr = '/';
		}
	}
	if (mkdir(retval, 0700) != 0 && errno != EEXIST) {
	error:
		SDL_SetError("Couldn't create directory '%s': '%s'", retval, strerror(errno));
		SDL_free(retval);
		return NULL;
	}

	// Append trailing /
	size_t final_len = SDL_strlen(retval);
	if (final_len + 1 < len) {
		retval[final_len++] = '/';
		retval[final_len] = '\0';
	}

	return retval;
#endif
}

#endif
