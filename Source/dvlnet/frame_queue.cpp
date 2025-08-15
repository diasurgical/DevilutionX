#include "dvlnet/frame_queue.h"

#include <cstring>

#include "appfat.h"
#include "dvlnet/packet.h"
#include "utils/attributes.h"
#include "utils/endian_read.hpp"
#include "utils/endian_write.hpp"

namespace devilution {
namespace net {

namespace {

PacketError FrameQueueError()
{
	return PacketError("Incorrect frame size");
}

} // namespace

framesize_t frame_queue::Size() const
{
	return current_size;
}

tl::expected<buffer_t, PacketError> frame_queue::Read(framesize_t s)
{
	if (current_size < s)
		return tl::make_unexpected(FrameQueueError());
	buffer_t ret;
	while (s > 0 && s >= buffer_deque.front().size()) {
		const framesize_t bufferSize = static_cast<framesize_t>(buffer_deque.front().size());
		s -= bufferSize;
		current_size -= bufferSize;
		ret.insert(ret.end(),
		    buffer_deque.front().begin(),
		    buffer_deque.front().end());
		buffer_deque.pop_front();
	}
	if (s > 0) {
		ret.insert(ret.end(),
		    buffer_deque.front().begin(),
		    buffer_deque.front().begin() + s);
		buffer_deque.front().erase(buffer_deque.front().begin(),
		    buffer_deque.front().begin() + s);
		current_size -= s;
	}
	return ret;
}

void frame_queue::Write(buffer_t buf)
{
	current_size += static_cast<framesize_t>(buf.size());
	buffer_deque.push_back(std::move(buf));
}

tl::expected<bool, PacketError> frame_queue::PacketReady()
{
	if (nextsize == 0) {
		if (Size() < sizeof(framesize_t))
			return false;
		tl::expected<buffer_t, PacketError> szbuf = Read(sizeof(framesize_t));
		if (!szbuf.has_value())
			return tl::make_unexpected(szbuf.error());
		nextsize = LoadLE32(szbuf->data());
		if (nextsize == 0)
			return tl::make_unexpected(FrameQueueError());
	}
	return Size() >= nextsize;
}

tl::expected<buffer_t, PacketError> frame_queue::ReadPacket()
{
	if (nextsize == 0 || Size() < nextsize)
		return tl::make_unexpected(FrameQueueError());
	tl::expected<buffer_t, PacketError> ret = Read(nextsize);
	nextsize = 0;
	return ret;
}

tl::expected<buffer_t, PacketError> frame_queue::MakeFrame(buffer_t packetbuf)
{
	buffer_t ret;
	const framesize_t size = static_cast<framesize_t>(packetbuf.size());
	if (size > max_frame_size)
		return tl::make_unexpected("Buffer exceeds maximum frame size");
	static_assert(sizeof(size) == 4, "framesize_t is not 4 bytes");
	unsigned char sizeBuf[4];
	WriteLE32(sizeBuf, size);
	ret.insert(ret.end(), sizeBuf, sizeBuf + 4);
	ret.insert(ret.end(), packetbuf.begin(), packetbuf.end());
	return ret;
}

} // namespace net
} // namespace devilution
