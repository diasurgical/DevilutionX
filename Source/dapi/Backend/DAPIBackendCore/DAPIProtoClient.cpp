#include <thread>

#include "DAPIProtoClient.h"

namespace DAPI {
DAPIProtoClient::DAPIProtoClient()
    : mt(static_cast<unsigned int>(std::chrono::system_clock::now().time_since_epoch().count()))
{
	udpbound = false;
	connectionPort = 1025;
}

void DAPIProtoClient::checkForConnection()
{
	if (isConnected())
		return;

	sf::Packet packet;
	udpSocket.setBlocking(false);
	if (!udpbound) {
		if (udpSocket.bind(1024, sf::IpAddress::Any) != sf::Socket::Status::Done)
			return;
		udpbound = true;
	}

	std::optional<sf::IpAddress> sender = sf::IpAddress::Any;
	auto port = udpSocket.getLocalPort();
	if (udpSocket.receive(packet, sender, port) != sf::Socket::Status::Done)
		return;

	auto size = packet.getDataSize();
	std::unique_ptr<char[]> packetContents(new char[size]);
	memcpy(packetContents.get(), packet.getData(), size);

	auto currentMessage = std::make_unique<dapi::message::Message>();
	currentMessage->ParseFromArray(packetContents.get(), static_cast<int>(size));

	if (!currentMessage->has_initbroadcast())
		return;

	auto reply = std::make_unique<dapi::message::Message>();
	auto initResponse = reply->mutable_initresponse();

	initResponse->set_port(static_cast<int>(connectionPort));

	packet.clear();
	size = reply->ByteSizeLong();
	std::unique_ptr<char[]> buffer(new char[size]);

	reply->SerializeToArray(&buffer[0], static_cast<int>(size));
	packet.append(buffer.get(), size);

	udpSocket.send(packet, sender.value(), port);
	udpSocket.unbind();
	udpbound = false;

	tcpListener.accept(tcpSocket);
	return;
}

void DAPIProtoClient::lookForServer()
{
	if (isConnected())
		return;

	sf::Packet packet;
	auto broadcastMessage = std::make_unique<dapi::message::Message>();
	broadcastMessage->mutable_initbroadcast();

	auto size = broadcastMessage->ByteSizeLong();
	std::unique_ptr<char[]> buffer(new char[size]);

	broadcastMessage->SerializeToArray(&buffer[0], size);
	packet.append(buffer.get(), size);

	std::optional<sf::IpAddress> server = sf::IpAddress::Broadcast;
	unsigned short port = 1024;

	udpSocket.send(packet, server.value(), port);
	server = sf::IpAddress::Any;
	udpSocket.setBlocking(false);
	// Sleep to give backend a chance to send the packet.
	{
		using namespace std::chrono_literals;
		std::this_thread::sleep_for(2s);
	}
	if (udpSocket.receive(packet, server, port) == sf::Socket::Status::Done) {
		size = packet.getDataSize();
		std::unique_ptr<char[]> replyBuffer(new char[size]);
		memcpy(replyBuffer.get(), packet.getData(), size);

		auto currentMessage = std::make_unique<dapi::message::Message>();
		currentMessage->ParseFromArray(replyBuffer.get(), size);

		if (!currentMessage->has_initresponse())
			return;

		connectionPort = static_cast<unsigned short>(currentMessage->initresponse().port());

		tcpSocket.connect(server.value(), connectionPort);
		if (!tcpSocket.getRemoteAddress().has_value())
			fprintf(stderr, "%s", "Connection failed.\n");
	}
}

void DAPIProtoClient::transmitMessages()
{
	// Check that we are connected to a game server.
	if (!isConnected())
		return;

	std::unique_ptr<dapi::message::Message> currentMessage;
	sf::Packet packet;
	// Loop until the message queue is empty.
	while (!messageQueue.empty()) {
		packet.clear();
		currentMessage = std::move(messageQueue.front());
		messageQueue.pop_front();
		auto size = currentMessage->ByteSizeLong();
		if (size > 0) {
			std::unique_ptr<char[]> buffer(new char[size]);
			currentMessage->SerializeToArray(&buffer[0], size);
			packet.append(buffer.get(), size);
		}
		if (tcpSocket.send(packet) != sf::Socket::Status::Done) {
			// Error sending message.
			fprintf(stderr, "Failed to send a Message. Disconnecting.\n");
			disconnect();
		}
	}

	// Finished with queue, send the EndOfQueue message.
	currentMessage = std::make_unique<dapi::message::Message>();
	currentMessage->mutable_endofqueue();
	packet.clear();
	auto size = currentMessage->ByteSizeLong();
	std::unique_ptr<char[]> buffer(new char[size]);
	currentMessage->SerializeToArray(&buffer[0], size);
	packet.append(buffer.get(), size);
	if (tcpSocket.send(packet) != sf::Socket::Status::Done) {
		// Error sending EndOfQueue
		fprintf(stderr, "Failed to send end of queue message. Disconnecting.\n");
		disconnect();
	}
}

void DAPIProtoClient::receiveMessages()
{
	// Check that we are connected to a game server or client.
	if (!isConnected())
		return;

	std::unique_ptr<dapi::message::Message> currentMessage;
	sf::Packet packet;
	// Loop until the end of queue message is received.
	while (true) {
		packet.clear();
		currentMessage = std::make_unique<dapi::message::Message>();
		if (tcpSocket.receive(packet) != sf::Socket::Status::Done) {
			fprintf(stderr, "Failed to receive message. Disconnecting.\n");
			disconnect();
			return;
		}
		auto size = packet.getDataSize();
		std::unique_ptr<char[]> packetContents(new char[size]);
		memcpy(packetContents.get(), packet.getData(), size);
		currentMessage->ParseFromArray(packetContents.get(), packet.getDataSize());
		if (currentMessage->has_endofqueue())
			return;
		messageQueue.push_back(std::move(currentMessage));
	}
}

void DAPIProtoClient::disconnect()
{
	if (!isConnected())
		return;
	tcpSocket.disconnect();
}

void DAPIProtoClient::initListen()
{
	tcpListener.setBlocking(true);
	while (tcpListener.listen(connectionPort) != sf::Socket::Status::Done)
		connectionPort = static_cast<unsigned short>(getRandomInteger(1025, 49151));
}

void DAPIProtoClient::stopListen()
{
	tcpListener.close();
}

void DAPIProtoClient::queueMessage(std::unique_ptr<dapi::message::Message> newMessage)
{
	messageQueue.push_back(std::move(newMessage));
}

std::unique_ptr<dapi::message::Message> DAPIProtoClient::getNextMessage()
{
	auto nextMessage = std::move(messageQueue.front());
	messageQueue.pop_front();
	return nextMessage;
}

bool DAPIProtoClient::isConnected() const
{
	return tcpSocket.getRemoteAddress().has_value();
}

int DAPIProtoClient::messageQueueSize() const
{
	return messageQueue.size();
}
} // namespace DAPI
