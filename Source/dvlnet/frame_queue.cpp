#include "dvlnet/frame_queue.h"

#include <cstring>

#include "appfat.h"
#include "dvlnet/packet.h"
#include "utils/attributes.h"
#include "utils/endian_read.hpp"

namespace devilution {
namespace net {

#if DVL_EXCEPTIONS
#define FRAME_QUEUE_ERROR throw frame_queue_exception()
#else
#define FRAME_QUEUE_ERROR app_fatal("frame queue error")
#endif

framesize_t frame_queue::Size() const
{
	return current_size;
}

buffer_t frame_queue::Read(framesize_t s)
{
	if (current_size < s)
		FRAME_QUEUE_ERROR;
	buffer_t ret;
	while (s > 0 && s >= buffer_deque.front().size()) {
		s -= buffer_deque.front().size();
		current_size -= buffer_deque.front().size();
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
	current_size += buf.size();
	buffer_deque.push_back(std::move(buf));
}

bool frame_queue::PacketReady()
{
	if (nextsize == 0) {
		if (Size() < sizeof(framesize_t))
			return false;
		auto szbuf = Read(sizeof(framesize_t));
		nextsize = LoadLE32(szbuf.data());
		if (nextsize == 0)
			FRAME_QUEUE_ERROR;
	}
	return Size() >= nextsize;
}

buffer_t frame_queue::ReadPacket()
{
	if (nextsize == 0 || Size() < nextsize)
		FRAME_QUEUE_ERROR;
	auto ret = Read(nextsize);
	nextsize = 0;
	return ret;
}

buffer_t frame_queue::MakeFrame(buffer_t packetbuf)
{
	buffer_t ret;
	if (packetbuf.size() > max_frame_size)
		ABORT();
	const framesize_t size = packetbuf.size();
	static_assert(sizeof(size) == 4, "framesize_t is not 4 bytes");
	unsigned char sizeBuf[4];
	WriteLE32(sizeBuf, size);
	ret.insert(ret.end(), sizeBuf, sizeBuf + 4);
	ret.insert(ret.end(), packetbuf.begin(), packetbuf.end());
	return ret;
}

} // namespace net
} // namespace devilution
