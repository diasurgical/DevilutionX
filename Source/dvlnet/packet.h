#pragma once

#include <array>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <type_traits>

#include <expected.hpp>

#ifdef PACKET_ENCRYPTION
#include <sodium.h>
#endif

#include "appfat.h"
#include "dvlnet/abstract_net.h"
#include "dvlnet/leaveinfo.hpp"
#include "utils/attributes.h"
#include "utils/endian_read.hpp"
#include "utils/endian_write.hpp"
#include "utils/str_cat.hpp"
#include "utils/stubs.h"

namespace devilution {
namespace net {

enum packet_type : uint8_t {
	// clang-format off
	PT_MESSAGE      = 0x01,
	PT_TURN         = 0x02,
	PT_JOIN_REQUEST = 0x11,
	PT_JOIN_ACCEPT  = 0x12,
	PT_CONNECT      = 0x13,
	PT_DISCONNECT   = 0x14,
	PT_INFO_REQUEST = 0x21,
	PT_INFO_REPLY   = 0x22,
	PT_ECHO_REQUEST = 0x31,
	PT_ECHO_REPLY   = 0x32,
	// clang-format on
};

// Returns NULL for an invalid packet type.
const char *packet_type_to_string(uint8_t packetType);

typedef uint8_t plr_t;
typedef uint8_t seq_t;
typedef uint32_t cookie_t;
typedef uint32_t timestamp_t;
#ifdef PACKET_ENCRYPTION
typedef std::array<unsigned char, crypto_secretbox_KEYBYTES> key_t;
#else
// Stub out the key_t definition as we're not doing any encryption.
using key_t = uint8_t;
#endif

struct turn_t {
	seq_t SequenceNumber;
	int32_t Value;
};

static constexpr plr_t PLR_MASTER = 0xFE;
static constexpr plr_t PLR_BROADCAST = 0xFF;

class PacketError {
public:
	enum class ErrorCode : uint8_t {
		None,
		EncryptionFailed,
		DecryptionFailed
	};

	PacketError()
	    : message_(std::string_view("Incorrect packet size"))
	    , code_(ErrorCode::None)
	{
	}

	PacketError(const char message[])
	    : message_(std::string_view(message))
	    , code_(ErrorCode::None)
	{
	}

	PacketError(std::string &&message)
	    : message_(std::move(message))
	    , code_(ErrorCode::None)
	{
	}

	PacketError(std::string_view message)
	    : message_(message)
	    , code_(ErrorCode::None)
	{
	}

	PacketError(ErrorCode code, const char message[])
	    : message_(std::string_view(message))
	    , code_(code)
	{
	}

	PacketError(ErrorCode code, std::string &&message)
	    : message_(std::move(message))
	    , code_(code)
	{
	}

	PacketError(ErrorCode code, std::string_view message)
	    : message_(message)
	    , code_(code)
	{
	}

	PacketError(const PacketError &error)
	    : message_(std::string(error.message_))
	    , code_(error.code_)
	{
	}

	PacketError(PacketError &&error)
	    : message_(std::move(error.message_))
	    , code_(error.code_)
	{
	}

	std::string_view what() const
	{
		return message_;
	}

	ErrorCode code() const
	{
		return code_;
	}

private:
	StringOrView message_;
	ErrorCode code_;
};

inline PacketError IoHandlerError(std::string message)
{
	return PacketError(std::move(message));
}

PacketError PacketTypeError(std::uint8_t unknownPacketType);
PacketError PacketTypeError(std::initializer_list<packet_type> expectedTypes, std::uint8_t actual);

class packet {
protected:
	packet_type m_type;
	plr_t m_src;
	plr_t m_dest;
	buffer_t m_message;
	turn_t m_turn;
	cookie_t m_cookie;
	plr_t m_newplr;
	timestamp_t m_time;
	buffer_t m_info;
	leaveinfo_t m_leaveinfo;

	const key_t &key;
	bool have_encrypted = false;
	bool have_decrypted = false;
	buffer_t encrypted_buffer;
	buffer_t decrypted_buffer;

public:
	packet(const key_t &k)
	    : key(k) { };

	const buffer_t &Data();

	packet_type Type();
	plr_t Source() const;
	plr_t Destination() const;
	tl::expected<const buffer_t *, PacketError> Message();
	tl::expected<turn_t, PacketError> Turn();
	tl::expected<cookie_t, PacketError> Cookie();
	tl::expected<plr_t, PacketError> NewPlayer();
	tl::expected<timestamp_t, PacketError> Time();
	tl::expected<const buffer_t *, PacketError> Info();
	tl::expected<leaveinfo_t, PacketError> LeaveInfo();
};

template <class P>
class packet_proc : public packet {
public:
	using packet::packet;
	tl::expected<void, PacketError> process_data();
};

class packet_in : public packet_proc<packet_in> {
public:
	using packet_proc<packet_in>::packet_proc;
	tl::expected<void, PacketError> Create(buffer_t buf);
	tl::expected<void, PacketError> process_element(buffer_t &x);
	template <class T>
	tl::expected<void, PacketError> process_element(T &x);
	tl::expected<void, PacketError> Decrypt(buffer_t buf);
};

class packet_out : public packet_proc<packet_out> {
public:
	using packet_proc<packet_out>::packet_proc;

	template <packet_type t, typename... Args>
	void create(Args... args);

	tl::expected<void, PacketError> process_element(buffer_t &x);
	template <class T>
	tl::expected<void, PacketError> process_element(const T &x);
	static cookie_t GenerateCookie();
	tl::expected<void, PacketError> Encrypt();
};

template <class P>
tl::expected<void, PacketError> packet_proc<P>::process_data()
{
	P &self = static_cast<P &>(*this);
	{
		tl::expected<void, PacketError> result
		    = self.process_element(m_type)
		          .and_then([&]() {
			          return self.process_element(m_src);
		          })
		          .and_then([&]() {
			          return self.process_element(m_dest);
		          });
		if (!result.has_value())
			return result;
	}
	switch (m_type) {
	case PT_MESSAGE:
		return self.process_element(m_message);
	case PT_TURN:
		return self.process_element(m_turn.SequenceNumber)
		    .and_then([&]() { return self.process_element(m_turn.Value); });
	case PT_JOIN_REQUEST:
		return self.process_element(m_cookie)
		    .and_then([&]() { return self.process_element(m_info); });
	case PT_JOIN_ACCEPT:
		return self.process_element(m_cookie)
		    .and_then([&]() { return self.process_element(m_newplr); })
		    .and_then([&]() { return self.process_element(m_info); });
	case PT_CONNECT:
		return self.process_element(m_newplr)
		    .and_then([&]() { return self.process_element(m_info); });
	case PT_DISCONNECT:
		return self.process_element(m_newplr)
		    .and_then([&]() { return self.process_element(m_leaveinfo); });
	case PT_INFO_REPLY:
		return self.process_element(m_info);
	case PT_INFO_REQUEST:
		return {};
	case PT_ECHO_REQUEST:
	case PT_ECHO_REPLY:
		return self.process_element(m_time);
	}
	return tl::make_unexpected(PacketTypeError(m_type));
}

inline tl::expected<void, PacketError> packet_in::process_element(buffer_t &x)
{
	x.insert(x.begin(), decrypted_buffer.begin(), decrypted_buffer.end());
	decrypted_buffer.resize(0);
	return {};
}

template <class T>
tl::expected<void, PacketError> packet_in::process_element(T &x)
{
	static_assert(std::is_integral<T>::value || std::is_enum<T>::value, "Unsupported T");
	static_assert(sizeof(T) == 4 || sizeof(T) == 2 || sizeof(T) == 1, "Unsupported T");
	if (decrypted_buffer.size() < sizeof(T)) {
		return tl::make_unexpected(PacketError());
	}
	if (sizeof(T) == 4) {
		x = static_cast<T>(LoadLE32(decrypted_buffer.data()));
	} else if (sizeof(T) == 2) {
		x = static_cast<T>(LoadLE16(decrypted_buffer.data()));
	} else if (sizeof(T) == 1) {
		std::memcpy(&x, decrypted_buffer.data(), sizeof(T));
	}
	decrypted_buffer.erase(decrypted_buffer.begin(),
	    decrypted_buffer.begin() + sizeof(T));
	return {};
}

template <>
inline void packet_out::create<PT_INFO_REQUEST>(plr_t s, plr_t d)
{
	if (have_encrypted || have_decrypted)
		ABORT();
	have_decrypted = true;
	m_type = PT_INFO_REQUEST;
	m_src = s;
	m_dest = d;
}

template <>
inline void packet_out::create<PT_INFO_REPLY>(plr_t s, plr_t d, buffer_t i)
{
	if (have_encrypted || have_decrypted)
		ABORT();
	have_decrypted = true;
	m_type = PT_INFO_REPLY;
	m_src = s;
	m_dest = d;
	m_info = std::move(i);
}

template <>
inline void packet_out::create<PT_MESSAGE>(plr_t s, plr_t d, buffer_t m)
{
	if (have_encrypted || have_decrypted)
		ABORT();
	have_decrypted = true;
	m_type = PT_MESSAGE;
	m_src = s;
	m_dest = d;
	m_message = std::move(m);
}

template <>
inline void packet_out::create<PT_TURN>(plr_t s, plr_t d, turn_t u)
{
	if (have_encrypted || have_decrypted)
		ABORT();
	have_decrypted = true;
	m_type = PT_TURN;
	m_src = s;
	m_dest = d;
	m_turn = u;
}

template <>
inline void packet_out::create<PT_JOIN_REQUEST>(plr_t s, plr_t d,
    cookie_t c, buffer_t i)
{
	if (have_encrypted || have_decrypted)
		ABORT();
	have_decrypted = true;
	m_type = PT_JOIN_REQUEST;
	m_src = s;
	m_dest = d;
	m_cookie = c;
	m_info = i;
}

template <>
inline void packet_out::create<PT_JOIN_ACCEPT>(plr_t s, plr_t d, cookie_t c,
    plr_t n, buffer_t i)
{
	if (have_encrypted || have_decrypted)
		ABORT();
	have_decrypted = true;
	m_type = PT_JOIN_ACCEPT;
	m_src = s;
	m_dest = d;
	m_cookie = c;
	m_newplr = n;
	m_info = i;
}

template <>
inline void packet_out::create<PT_CONNECT>(plr_t s, plr_t d, plr_t n, buffer_t i)
{
	if (have_encrypted || have_decrypted)
		ABORT();
	have_decrypted = true;
	m_type = PT_CONNECT;
	m_src = s;
	m_dest = d;
	m_newplr = n;
	m_info = i;
}

template <>
inline void packet_out::create<PT_CONNECT>(plr_t s, plr_t d, plr_t n)
{
	if (have_encrypted || have_decrypted)
		ABORT();
	have_decrypted = true;
	m_type = PT_CONNECT;
	m_src = s;
	m_dest = d;
	m_newplr = n;
}

template <>
inline void packet_out::create<PT_DISCONNECT>(plr_t s, plr_t d, plr_t n,
    leaveinfo_t l)
{
	if (have_encrypted || have_decrypted)
		ABORT();
	have_decrypted = true;
	m_type = PT_DISCONNECT;
	m_src = s;
	m_dest = d;
	m_newplr = n;
	m_leaveinfo = l;
}

template <>
inline void packet_out::create<PT_ECHO_REQUEST>(plr_t s, plr_t d, timestamp_t t)
{
	if (have_encrypted || have_decrypted)
		ABORT();
	have_decrypted = true;
	m_type = PT_ECHO_REQUEST;
	m_src = s;
	m_dest = d;
	m_time = t;
}

template <>
inline void packet_out::create<PT_ECHO_REPLY>(plr_t s, plr_t d, timestamp_t t)
{
	if (have_encrypted || have_decrypted)
		ABORT();
	have_decrypted = true;
	m_type = PT_ECHO_REPLY;
	m_src = s;
	m_dest = d;
	m_time = t;
}

inline tl::expected<void, PacketError> packet_out::process_element(buffer_t &x)
{
	decrypted_buffer.insert(decrypted_buffer.end(), x.begin(), x.end());
	return {};
}

template <class T>
tl::expected<void, PacketError> packet_out::process_element(const T &x)
{
	static_assert(std::is_integral<T>::value || std::is_enum<T>::value, "Unsupported T");
	static_assert(sizeof(T) == 4 || sizeof(T) == 2 || sizeof(T) == 1, "Unsupported T");
	if (sizeof(T) == 4) {
		unsigned char buf[4];
		if constexpr (std::is_enum<T>::value) {
			WriteLE32(buf, static_cast<std::underlying_type_t<T>>(x));
		} else {
			WriteLE32(buf, x);
		}
		decrypted_buffer.insert(decrypted_buffer.end(), buf, buf + 4);
	} else if (sizeof(T) == 2) {
		unsigned char buf[2];
		if constexpr (std::is_enum<T>::value) {
			WriteLE16(buf, static_cast<std::underlying_type_t<T>>(x));
		} else {
			WriteLE16(buf, x);
		}
		decrypted_buffer.insert(decrypted_buffer.end(), buf, buf + 2);
	} else if (sizeof(T) == 1) {
		decrypted_buffer.push_back(static_cast<unsigned char>(x));
	}
	return {};
}

class packet_factory {
	key_t key = {};
	bool secure;

public:
	static constexpr unsigned short max_packet_size = 0xFFFF;

	packet_factory();
	packet_factory(std::string pw);
	tl::expected<std::unique_ptr<packet>, PacketError> make_packet(buffer_t buf);
	template <packet_type t, typename... Args>
	tl::expected<std::unique_ptr<packet>, PacketError> make_packet(Args... args);
};

inline tl::expected<std::unique_ptr<packet>, PacketError> packet_factory::make_packet(buffer_t buf)
{
	auto ret = std::make_unique<packet_in>(key);
#ifndef PACKET_ENCRYPTION
	tl::expected<void, PacketError> isCreated = ret->Create(std::move(buf));
#else
	tl::expected<void, PacketError> isCreated = !secure
	    ? ret->Create(std::move(buf))
	    : ret->Decrypt(std::move(buf));
#endif
	if (!isCreated.has_value()) {
		return tl::make_unexpected(isCreated.error());
	}
	if (const tl::expected<void, PacketError> result = ret->process_data(); !result.has_value()) {
		return tl::make_unexpected(result.error());
	}
	return ret;
}

template <packet_type t, typename... Args>
tl::expected<std::unique_ptr<packet>, PacketError> packet_factory::make_packet(Args... args)
{
	auto ret = std::make_unique<packet_out>(key);
	ret->create<t>(args...);
	if (const tl::expected<void, PacketError> result = ret->process_data(); !result.has_value()) {
		return tl::make_unexpected(result.error());
	}
#ifdef PACKET_ENCRYPTION
	if (secure) {
		tl::expected<void, PacketError> isEncrypted = ret->Encrypt();
		if (!isEncrypted.has_value()) {
			return tl::make_unexpected(isEncrypted.error());
		}
	}
#endif
	return ret;
}

} // namespace net
} // namespace devilution
