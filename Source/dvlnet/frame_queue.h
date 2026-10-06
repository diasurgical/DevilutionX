#pragma once

#include <cstdint>
#include <deque>
#include <exception>
#include <expected>
#include <vector>

#include "dvlnet/packet.h"

namespace devilution {
namespace net {

typedef std::vector<unsigned char> buffer_t;
typedef uint32_t framesize_t;

class frame_queue {
public:
	constexpr static framesize_t frame_size_mask = 0xFFFF;
	constexpr static framesize_t max_frame_size = 0xFFFF;

private:
	framesize_t current_size = 0;
	std::deque<buffer_t> buffer_deque;
	framesize_t nextsize = 0;

	framesize_t Size() const;
	std::expected<buffer_t, PacketError> Read(framesize_t s);

public:
	std::expected<bool, PacketError> PacketReady();
	uint16_t ReadPacketFlags();
	std::expected<buffer_t, PacketError> ReadPacket();
	void Write(buffer_t buf);

	static std::expected<buffer_t, PacketError> MakeFrame(buffer_t packetbuf, uint16_t flags = 0);
};

} // namespace net
} // namespace devilution
