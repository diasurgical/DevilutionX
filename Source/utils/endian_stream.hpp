#pragma once

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "utils/endian_read.hpp"
#include "utils/endian_write.hpp"
#include "utils/log.hpp"

namespace devilution {

inline void LoggedFread(void *buffer, size_t size, FILE *stream)
{
	if (std::fread(buffer, size, 1, stream) != 1 && !std::feof(stream)) {
		LogError("fread failed: {}", std::strerror(errno));
	}
}

inline void LoggedFwrite(const void *buffer, size_t size, FILE *stream)
{
	if (std::fwrite(buffer, size, 1, stream) != 1) {
		LogError("fwrite failed: {}", std::strerror(errno));
	}
}

template <typename T = uint8_t>
T ReadByte(FILE *stream)
{
	static_assert(sizeof(T) == 1, "invalid argument");
	char buf;
	LoggedFread(&buf, sizeof(buf), stream);
	return static_cast<T>(buf);
}

template <typename T = uint16_t>
T ReadLE16(FILE *stream)
{
	static_assert(sizeof(T) == 2, "invalid argument");
	char buf[2];
	LoggedFread(&buf, sizeof(buf), stream);
	return static_cast<T>(LoadLE16(buf));
}

template <typename T = uint32_t>
T ReadLE32(FILE *stream)
{
	static_assert(sizeof(T) == 4, "invalid argument");
	char buf[4];
	LoggedFread(&buf, sizeof(buf), stream);
	return static_cast<T>(LoadLE32(buf));
}

inline float ReadLEFloat(FILE *stream)
{
	static_assert(sizeof(float) == sizeof(uint32_t), "invalid float size");
	const uint32_t val = ReadLE32(stream);
	float result;
	memcpy(&result, &val, sizeof(float));
	return result;
}

inline void WriteByte(FILE *out, uint8_t val)
{
	LoggedFwrite(&val, sizeof(val), out);
}

inline void WriteLE16(FILE *out, uint16_t val)
{
	char data[2];
	WriteLE16(data, val);
	LoggedFwrite(data, sizeof(data), out);
}

inline void WriteLE32(FILE *out, uint32_t val)
{
	char data[4];
	WriteLE32(data, val);
	LoggedFwrite(data, sizeof(data), out);
}

inline void WriteLEFloat(FILE *out, float val)
{
	static_assert(sizeof(float) == sizeof(uint32_t), "invalid float size");
	uint32_t intVal;
	memcpy(&intVal, &val, sizeof(uint32_t));
	char data[4];
	WriteLE32(data, intVal);
	LoggedFwrite(data, sizeof(data), out);
}

} // namespace devilution
