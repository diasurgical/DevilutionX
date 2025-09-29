#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "appfat.h"
#include "sha.h"
#include "utils/endian_read.hpp"
#include "utils/log.hpp"
#include "utils/stdcompat/cstddef.hpp"

namespace devilution {
namespace {

struct CodecSignature {
	uint32_t checksum;
	uint8_t error;
	uint8_t lastChunkSize;
};

constexpr size_t BlockSizeBytes = BlockSize * sizeof(uint32_t);
constexpr size_t SignatureSize = 8;

SHA1Context CodecInitKey(const char *pszPassword)
{
	uint32_t pw[BlockSize]; // Repeat password until 64 char long
	std::size_t j = 0;
	for (uint32_t &value : pw) {
		if (pszPassword[j] == '\0')
			j = 0;
		value = LoadLE32(&pszPassword[j]);
		j += sizeof(uint32_t);
	}

	uint32_t digest[SHA1HashSize];
	{
		SHA1Context context;
		SHA1Calculate(context, pw);
		SHA1Result(context, digest);
	}

	uint32_t key[BlockSize] {
		2908958655, 4146550480, 658981742, 1113311088, 3927878744, 679301322, 1760465731, 3305370375,
		2269115995, 3928541685, 580724401, 2607446661, 2233092279, 2416822349, 4106933702, 3046442503
	};

	for (unsigned i = 0; i < BlockSize; ++i) {
		key[i] ^= digest[(i + 3) % SHA1HashSize];
	}

	SHA1Context context;
	SHA1Calculate(context, key);
	return context;
}

CodecSignature GetCodecSignature(byte *src)
{
	CodecSignature result;
	result.checksum = LoadLE32(src);
	src += 4;
	result.error = static_cast<uint8_t>(*src++);
	result.lastChunkSize = static_cast<uint8_t>(*src);
	return result;
}

void SetCodecSignature(byte *dst, CodecSignature sig)
{
	*dst++ = static_cast<byte>(sig.checksum);
	*dst++ = static_cast<byte>(sig.checksum >> 8);
	*dst++ = static_cast<byte>(sig.checksum >> 16);
	*dst++ = static_cast<byte>(sig.checksum >> 24);
	*dst++ = static_cast<byte>(sig.error);
	*dst++ = static_cast<byte>(sig.lastChunkSize);
	*dst++ = static_cast<byte>(0);
	*dst++ = static_cast<byte>(0);
}

void ByteSwapBlock(uint32_t *data)
{
	for (size_t i = 0; i < BlockSize; ++i)
		data[i] = SDL_SwapLE32(data[i]);
}

void XorBlock(const uint32_t *shaResult, uint32_t *out)
{
	for (unsigned i = 0; i < BlockSize; ++i)
		out[i] ^= shaResult[i % SHA1HashSize];
}

} // namespace

std::size_t codec_decode(byte *pbSrcDst, std::size_t size, const char *pszPassword)
{
	uint32_t buf[BlockSize];
	uint32_t dst[SHA1HashSize];

	SHA1Context context = CodecInitKey(pszPassword);
	if (size <= SignatureSize)
		return 0;
	size -= SignatureSize;
	if (size % BlockSize != 0)
		return 0;
	for (size_t i = 0; i < size; pbSrcDst += BlockSizeBytes, i += BlockSizeBytes) {
		memcpy(buf, pbSrcDst, BlockSizeBytes);
		ByteSwapBlock(buf);
		SHA1Result(context, dst);
		XorBlock(dst, buf);
		SHA1Calculate(context, buf);
		ByteSwapBlock(buf);
		memcpy(pbSrcDst, buf, BlockSizeBytes);
	}

	memset(buf, 0, sizeof(buf));
	const CodecSignature sig = GetCodecSignature(pbSrcDst);
	if (sig.error > 0) {
		return 0;
	}

	SHA1Result(context, dst);
	if (sig.checksum != dst[0]) {
		LogError("Checksum mismatch signature={} vs calculated={}", sig.checksum, dst[0]);
		memset(dst, 0, sizeof(dst));
		return 0;
	}

	size += sig.lastChunkSize - BlockSizeBytes;
	return size;
}

std::size_t codec_get_encoded_len(std::size_t dwSrcBytes)
{
	if (dwSrcBytes % BlockSizeBytes != 0)
		dwSrcBytes += BlockSizeBytes - (dwSrcBytes % BlockSizeBytes);
	return dwSrcBytes + SignatureSize;
}

void codec_encode(byte *pbSrcDst, std::size_t size, std::size_t size64, const char *pszPassword)
{
	uint32_t buf[BlockSize];
	uint32_t tmp[SHA1HashSize];
	uint32_t dst[SHA1HashSize];

	if (size64 != codec_get_encoded_len(size))
		app_fatal("Invalid encode parameters");
	SHA1Context context = CodecInitKey(pszPassword);

	size_t lastChunk = 0;
	while (size != 0) {
		const size_t chunk = std::min(size, BlockSizeBytes);
		memset(buf, 0, sizeof(buf));
		memcpy(buf, pbSrcDst, chunk);
		ByteSwapBlock(buf);
		SHA1Result(context, dst);
		SHA1Calculate(context, buf);
		XorBlock(dst, buf);
		ByteSwapBlock(buf);
		memcpy(pbSrcDst, buf, BlockSizeBytes);
		pbSrcDst += BlockSizeBytes;
		lastChunk = chunk;
		size -= chunk;
	}
	memset(buf, 0, sizeof(buf));
	SHA1Result(context, tmp);
	SetCodecSignature(pbSrcDst, CodecSignature { /*.checksum=*/*reinterpret_cast<uint32_t *>(tmp),
	                                /*.error=*/0,
	                                // lastChunk is at most 64 so will always fit in an 8 bit var
	                                /*.lastChunkSize=*/static_cast<uint8_t>(lastChunk) });
}

} // namespace devilution
