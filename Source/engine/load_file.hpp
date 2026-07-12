#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <memory>

#include "appfat.h"
#include "engine/assets.hpp"
#include "headless_mode.hpp"
#include "mpq/mpq_common.hpp"
#include "utils/static_vector.hpp"
#include "utils/str_cat.hpp"

namespace devilution {

template <typename T>
std::expected<void, std::string> LoadFileInMemWithStatus(const char *path, T *data)
{
	size_t size;
	AssetHandle handle = OpenAsset(path, size);
	if (!handle.ok()) {
		if (HeadlessMode) return {};
		return std::unexpected(FailedToOpenFileErrorMessage(path, handle.error()));
	}
	if ((size % sizeof(T)) != 0) {
		return std::unexpected(StrCat("File size does not align with type\n", path));
	}
	if (!handle.read(data, size)) {
		return std::unexpected("handle.read failed");
	}
	return {};
}

template <typename T>
std::expected<void, std::string> LoadIntegralFileInMemWithStatus(const char *path, T *data)
{
	size_t size;
	AssetHandle handle = OpenIntegralAsset(path, size);
	if (!handle.ok()) {
		if (HeadlessMode) return {};
		return std::unexpected(FailedToOpenFileErrorMessage(path, handle.error()));
	}
	if ((size % sizeof(T)) != 0) {
		return std::unexpected(StrCat("File size does not align with type\n", path));
	}
	if (!handle.read(data, size)) {
		return std::unexpected("handle.read failed");
	}
	return {};
}

template <typename T>
void LoadFileInMem(const char *path, T *data)
{
	const std::expected<void, std::string> result = LoadFileInMemWithStatus<T>(path, data);
	if (!result.has_value()) app_fatal(result.error());
}

template <typename T>
std::expected<void, std::string> LoadFileInMemWithStatus(const char *path, T *data, std::size_t count)
{
	AssetHandle handle = OpenAsset(path);
	if (!handle.ok()) {
		if (HeadlessMode) return {};
		return std::unexpected(FailedToOpenFileErrorMessage(path, handle.error()));
	}
	if (!handle.read(data, count * sizeof(T))) {
		return std::unexpected("handle.read failed");
	}
	return {};
}

template <typename T>
void LoadFileInMem(const char *path, T *data, std::size_t count)
{
	std::expected<void, std::string> result = LoadFileInMemWithStatus<T>(path, data, count);
	if (!result.has_value()) app_fatal(result.error());
}

template <typename T>
bool LoadOptionalFileInMem(const char *path, T *data, std::size_t count)
{
	AssetHandle handle = OpenAsset(path);
	return handle.ok() && handle.read(data, count * sizeof(T));
}

template <typename T, std::size_t N>
std::expected<void, std::string> LoadFileInMemWithStatus(const char *path, std::array<T, N> &data)
{
	return LoadFileInMemWithStatus(path, data.data(), N);
}

template <typename T, std::size_t N>
void LoadFileInMem(const char *path, std::array<T, N> &data)
{
	LoadFileInMem(path, data.data(), N);
}

template <typename T = std::byte>
std::expected<std::unique_ptr<T[]>, std::string> LoadFileInMemWithStatus(const char *path, std::size_t *numRead = nullptr)
{
	size_t size;
	AssetHandle handle = OpenAsset(path, size);
	if (!handle.ok()) {
		if (HeadlessMode) return {};
		return std::unexpected(FailedToOpenFileErrorMessage(path, handle.error()));
	}
	if ((size % sizeof(T)) != 0) {
		return std::unexpected(StrCat("File size does not align with type\n", path));
	}

	if (numRead != nullptr)
		*numRead = size / sizeof(T);

	std::unique_ptr<T[]> buf { new T[size / sizeof(T)] };
	if (!handle.read(buf.get(), size)) {
		return std::unexpected("handle.read failed");
	}
	return { std::move(buf) };
}

/**
 * @brief Load a file in to a buffer
 * @param path Path of file
 * @param numRead Number of T elements read
 * @return Buffer with content of file
 */
template <typename T = std::byte>
std::unique_ptr<T[]> LoadFileInMem(const char *path, std::size_t *numRead = nullptr)
{
	std::expected<std::unique_ptr<T[]>, std::string> result = LoadFileInMemWithStatus<T>(path, numRead);
	if (!result.has_value()) app_fatal(result.error());
	return std::move(result).value();
}

/**
 * @brief Reads multiple files into a single buffer
 *
 * @tparam MaxFiles maximum number of files
 */
template <size_t MaxFiles>
struct MultiFileLoader {
	struct DefaultFilterFn {
		bool operator()(size_t i) const
		{
			return true;
		}
	};

	/**
	 * @param numFiles number of files to read
	 * @param pathFn a function that returns the path for the given index
	 * @param outOffsets a buffer index for the start of each file will be written here, then the total file size at the end.
	 * @param filterFn a function that returns whether to load a file for the given index
	 * @return std::unique_ptr<std::byte[]> the buffer with all the files
	 */
	template <typename PathFn, typename FilterFn = DefaultFilterFn>
	[[nodiscard]] std::unique_ptr<std::byte[]> operator()(size_t numFiles, PathFn &&pathFn, uint32_t *outOffsets,
	    FilterFn filterFn = DefaultFilterFn {})
	{
		StaticVector<std::array<char, MaxMpqPathSize>, MaxFiles> paths;
		StaticVector<AssetRef, MaxFiles> files;
		StaticVector<uint32_t, MaxFiles> sizes;
		size_t totalSize = 0;
		for (size_t i = 0, j = 0; i < numFiles; ++i) {
			if (!filterFn(i))
				continue;
			{
				const char *path = pathFn(i);
				paths.emplace_back();
				memcpy(paths.back().data(), path, strlen(path) + 1);
			}
			const char *path = paths.back().data();
			files.emplace_back(FindAsset(path));
			if (!ValidatAssetRef(path, files.back()))
				return nullptr;

			const size_t size = files.back().size();
			sizes.emplace_back(static_cast<uint32_t>(size));
			outOffsets[j] = static_cast<uint32_t>(totalSize);
			totalSize += size;
			++j;
		}
		outOffsets[files.size()] = static_cast<uint32_t>(totalSize);
		std::unique_ptr<std::byte[]> buf { new std::byte[totalSize] };
		for (size_t i = 0, j = 0; i < numFiles; ++i) {
			if (!filterFn(i))
				continue;
			AssetHandle handle = OpenAsset(std::move(files[j]));
			if (!handle.ok() || !handle.read(&buf[outOffsets[j]], sizes[j])) {
				FailedToOpenFileError(paths[j].data(), handle.error());
			}
			++j;
		}
		return buf;
	}
};

} // namespace devilution
