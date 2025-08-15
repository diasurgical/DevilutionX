#include "dvlnet/packet.h"

#include <cassert>
#include <cstdint>

#ifdef PACKET_ENCRYPTION
#include <sodium.h>
#else
#include <chrono>
#include <random>
#endif

#include <expected.hpp>

#include "utils/algorithm/container.hpp"
#include "utils/str_cat.hpp"

namespace devilution::net {

#ifdef PACKET_ENCRYPTION

cookie_t packet_out::GenerateCookie()
{
	cookie_t cookie;
	randombytes_buf(reinterpret_cast<unsigned char *>(&cookie),
	    sizeof(cookie_t));
	return cookie;
}

#else

class cookie_generator {
public:
	cookie_generator()
	{
		unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
		generator.seed(seed);
	}

	cookie_t NewCookie()
	{
		return distribution(generator);
	}

private:
	std::default_random_engine generator;
	std::uniform_int_distribution<cookie_t> distribution;
};

cookie_generator CookieGenerator;

cookie_t packet_out::GenerateCookie()
{
	return CookieGenerator.NewCookie();
}

#endif

const char *packet_type_to_string(uint8_t packetType)
{
	switch (packetType) {
	case PT_MESSAGE:
		return "PT_MESSAGE";
	case PT_TURN:
		return "PT_TURN";
	case PT_JOIN_REQUEST:
		return "PT_JOIN_REQUEST";
	case PT_JOIN_ACCEPT:
		return "PT_JOIN_ACCEPT";
	case PT_CONNECT:
		return "PT_CONNECT";
	case PT_DISCONNECT:
		return "PT_DISCONNECT";
	case PT_INFO_REQUEST:
		return "PT_INFO_REQUEST";
	case PT_INFO_REPLY:
		return "PT_INFO_REPLY";
	case PT_ECHO_REQUEST:
		return "PT_ECHO_REQUEST";
	case PT_ECHO_REPLY:
		return "PT_ECHO_REPLY";
	default:
		return nullptr;
	}
}

PacketError PacketTypeError(std::uint8_t unknownPacketType)
{
	return PacketError(StrCat("Unknown packet type ", unknownPacketType));
}

PacketError PacketTypeError(std::initializer_list<packet_type> expectedTypes, std::uint8_t actual)
{
	std::string message = "Expected packet of type ";
	const auto appendPacketType = [&](std::uint8_t t) {
		const char *typeStr = packet_type_to_string(t);
		if (typeStr != nullptr)
			message.append(typeStr);
		else
			StrAppend(message, t);
	};

	constexpr char KJoinTypes[] = " or ";
	for (const packet_type t : expectedTypes) {
		appendPacketType(t);
		message.append(KJoinTypes);
	}
	message.resize(message.size() - (sizeof(KJoinTypes) - 1));
	message.append(", got");
	appendPacketType(actual);
	return PacketError(std::move(message));
}

namespace {

tl::expected<void, PacketError> CheckPacketTypeOneOf(std::initializer_list<packet_type> expectedTypes, std::uint8_t actualType)
{
	if (c_none_of(expectedTypes,
	        [actualType](uint8_t type) { return type == actualType; })) {
		return tl::make_unexpected(PacketTypeError(expectedTypes, actualType));
	}
	return {};
}

} // namespace

const buffer_t &packet::Data()
{
	assert(have_encrypted || have_decrypted);
	if (have_encrypted)
		return encrypted_buffer;
	return decrypted_buffer;
}

packet_type packet::Type()
{
	assert(have_decrypted);
	return m_type;
}

plr_t packet::Source() const
{
	assert(have_decrypted);
	return m_src;
}

plr_t packet::Destination() const
{
	assert(have_decrypted);
	return m_dest;
}

tl::expected<const buffer_t *, PacketError> packet::Message()
{
	assert(have_decrypted);
	return CheckPacketTypeOneOf({ PT_MESSAGE }, m_type)
	    .transform([this]() { return &m_message; });
}

tl::expected<turn_t, PacketError> packet::Turn()
{
	assert(have_decrypted);
	return CheckPacketTypeOneOf({ PT_TURN }, m_type)
	    .transform([this]() { return m_turn; });
}

tl::expected<cookie_t, PacketError> packet::Cookie()
{
	assert(have_decrypted);
	return CheckPacketTypeOneOf({ PT_JOIN_REQUEST, PT_JOIN_ACCEPT }, m_type)
	    .transform([this]() { return m_cookie; });
}

tl::expected<plr_t, PacketError> packet::NewPlayer()
{
	assert(have_decrypted);
	return CheckPacketTypeOneOf({ PT_JOIN_ACCEPT, PT_CONNECT, PT_DISCONNECT }, m_type)
	    .transform([this]() { return m_newplr; });
}

tl::expected<timestamp_t, PacketError> packet::Time()
{
	assert(have_decrypted);
	return CheckPacketTypeOneOf({ PT_ECHO_REQUEST, PT_ECHO_REPLY }, m_type)
	    .transform([this]() { return m_time; });
}

tl::expected<const buffer_t *, PacketError> packet::Info()
{
	assert(have_decrypted);
	return CheckPacketTypeOneOf({ PT_JOIN_REQUEST, PT_JOIN_ACCEPT, PT_CONNECT, PT_INFO_REPLY }, m_type)
	    .transform([this]() { return &m_info; });
}

tl::expected<leaveinfo_t, PacketError> packet::LeaveInfo()
{
	assert(have_decrypted);
	return CheckPacketTypeOneOf({ PT_DISCONNECT }, m_type)
	    .transform([this]() { return m_leaveinfo; });
}

tl::expected<void, PacketError> packet_in::Create(buffer_t buf)
{
	assert(!have_encrypted && !have_decrypted);
	if (buf.size() < sizeof(packet_type) + 2 * sizeof(plr_t))
		return tl::make_unexpected(PacketError());

	decrypted_buffer = std::move(buf);
	have_decrypted = true;

	// TCP server implementation forwards the original data to clients
	// so although we are not decrypting anything,
	// we save a copy in encrypted_buffer anyway
	encrypted_buffer = decrypted_buffer;
	have_encrypted = true;
	return {};
}

#ifdef PACKET_ENCRYPTION
tl::expected<void, PacketError> packet_in::Decrypt(buffer_t buf)
{
	assert(!have_encrypted && !have_decrypted);
	encrypted_buffer = std::move(buf);
	have_encrypted = true;

	if (encrypted_buffer.size() < crypto_secretbox_NONCEBYTES
	        + crypto_secretbox_MACBYTES
	        + sizeof(packet_type) + 2 * sizeof(plr_t))
		return tl::make_unexpected(PacketError());
	auto pktlen = (encrypted_buffer.size()
	    - crypto_secretbox_NONCEBYTES
	    - crypto_secretbox_MACBYTES);
	decrypted_buffer.resize(pktlen);
	const int status = crypto_secretbox_open_easy(
	    decrypted_buffer.data(),
	    encrypted_buffer.data() + crypto_secretbox_NONCEBYTES,
	    encrypted_buffer.size() - crypto_secretbox_NONCEBYTES,
	    encrypted_buffer.data(),
	    key.data());
	if (status != 0)
		return tl::make_unexpected(PacketError());

	have_decrypted = true;
	return {};
}
#endif

#ifdef PACKET_ENCRYPTION
void packet_out::Encrypt()
{
	assert(have_decrypted);

	if (have_encrypted)
		return;

	auto lenCleartext = decrypted_buffer.size();
	encrypted_buffer.insert(encrypted_buffer.begin(),
	    crypto_secretbox_NONCEBYTES, 0);
	encrypted_buffer.insert(encrypted_buffer.end(),
	    crypto_secretbox_MACBYTES + lenCleartext, 0);
	randombytes_buf(encrypted_buffer.data(), crypto_secretbox_NONCEBYTES);
	const int status = crypto_secretbox_easy(
	    encrypted_buffer.data() + crypto_secretbox_NONCEBYTES,
	    decrypted_buffer.data(),
	    lenCleartext,
	    encrypted_buffer.data(),
	    key.data());
	if (status != 0)
		ABORT();

	have_encrypted = true;
}
#endif

packet_factory::packet_factory()
{
	secure = false;
}

packet_factory::packet_factory(std::string pw)
{
	secure = false;

#ifdef PACKET_ENCRYPTION
	if (sodium_init() < 0)
		ABORT();
	pw.resize(std::min<std::size_t>(pw.size(), crypto_pwhash_argon2id_PASSWD_MAX));
	pw.resize(std::max<std::size_t>(pw.size(), crypto_pwhash_argon2id_PASSWD_MIN), 0);
	std::string salt("W9bE9dQgVaeybwr2");
	salt.resize(crypto_pwhash_argon2id_SALTBYTES, 0);
	const int status = crypto_pwhash(
	    key.data(),
	    crypto_secretbox_KEYBYTES,
	    pw.data(),
	    pw.size(),
	    reinterpret_cast<const unsigned char *>(salt.data()),
	    3 * crypto_pwhash_argon2id_OPSLIMIT_MIN,
	    2 * crypto_pwhash_argon2id_MEMLIMIT_MIN,
	    crypto_pwhash_ALG_ARGON2ID13);
	if (status != 0)
		ABORT();
	secure = true;
#endif
}

} // namespace devilution::net
