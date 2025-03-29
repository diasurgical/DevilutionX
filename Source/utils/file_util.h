#pragma once

#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace devilution {

#ifdef _WIN32
constexpr char DirectorySeparator = '\\';
#define DIRECTORY_SEPARATOR_STR "\\"
#else
constexpr char DirectorySeparator = '/';
#define DIRECTORY_SEPARATOR_STR "/"
#endif

bool FileExists(const char *path);

inline bool FileExists(const std::string &str)
{
	return FileExists(str.c_str());
}

bool DirectoryExists(const char *path);
std::string_view Dirname(std::string_view path);
bool FileExistsAndIsWriteable(const char *path);
bool GetFileSize(const char *path, std::uintmax_t *size);

/**
 * @brief Creates a single directory (non-recursively).
 *
 * @return True if the directory already existed or has been created successfully.
 */
bool CreateDir(const char *path);

void RecursivelyCreateDir(const char *path);
bool ResizeFile(const char *path, std::uintmax_t size);
void RenameFile(const char *from, const char *to);
void CopyFileOverwrite(const char *from, const char *to);
void RemoveFile(const char *path);
FILE *OpenFile(const char *path, const char *mode);

#if defined(_WIN32) && !defined(DEVILUTIONX_WINDOWS_NO_WCHAR)
std::unique_ptr<wchar_t[]> ToWideChar(std::string_view path);
#endif

std::vector<std::string> ListDirectories(const char *path);
std::vector<std::string> ListFiles(const char *path);

} // namespace devilution
