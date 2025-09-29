#pragma once

#include <cstddef>
#include <cstdint>

#include "utils/endian_read.hpp"

namespace devilution {

constexpr size_t MaxMpqPathSize = 256;

#pragma pack(push, 1)
struct MpqFileHeader {
	static constexpr uint32_t DiabloSignature = LoadLE32("MPQ\x1A");
	static constexpr uint32_t DiabloSize = 32;

	// The signature, always 0x1A51504D ('MPQ\x1A') for Diablo MPQs.
	uint32_t signature;

	// The size of this header in bytes, always 32 for Diablo MPQs.
	uint32_t headerSize;

	// The size of the MPQ file in bytes.
	uint32_t fileSize;

	// Version, always 0 for Diablo MPQs.
	uint16_t version;

	// Block size is `512 * 2 ^ blockSizeFactor`.
	// e.g. if `blockSizeFactor` is 3, the block size is 4096 (512 << 3).
	uint16_t blockSizeFactor;

	// Location of the hash entries table.
	uint32_t hashEntriesOffset;

	// Location of the block entries table.
	uint32_t blockEntriesOffset;

	// Size of the hash entries table (number of entries).
	uint32_t hashEntriesCount;

	// Size of the block entries table (number of entries).
	uint32_t blockEntriesCount;

	// Empty space after the header. Not included into `headerSize`.
	uint8_t pad[72];
};

struct MpqHashEntry {
	// Special values for the `block` field.
	// Does not point to a block (unassigned hash entry)
	static constexpr uint32_t NullBlock = -1;

	// Used to point to a block but is now deleted (can be reclaimed)
	static constexpr uint32_t DeletedBlock = -2;

	// `hashA` and `hashB` are used for resolving hash index collisions.
	uint32_t hashA;
	uint32_t hashB;

	// Always `0` in Diablo.
	uint16_t locale;

	// Always `0` in Diablo.
	uint16_t platform;

	// Index of the first block in the block entries table, or
	// -1 for an unused entry, -2 for a deleted entry.
	uint32_t block;
};

struct MpqBlockEntry {
	static constexpr uint32_t FlagExists = 0x80000000;
	static constexpr uint32_t CompressPkZip = 0x00000100;

	// Offset to the start of this block.
	uint32_t offset;

	// Size in the MPQ.
	uint32_t packedSize;

	// Uncompressed size.
	uint32_t unpackedSize;

	// Flags indicating compression type, encryption, etc.
	uint32_t flags;
};
#pragma pack(pop)

} // namespace devilution
