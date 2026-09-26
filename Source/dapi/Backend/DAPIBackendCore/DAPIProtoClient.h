#pragma once

#include <deque>
#include <random>

#include <SFML/Network.hpp>

#include "message.pb.h"

namespace DAPI {
struct DAPIProtoClient {
	DAPIProtoClient();

	void checkForConnection();
	void lookForServer();
	void transmitMessages();
	void receiveMessages();
	void disconnect();
	void initListen();
	void stopListen();

	void queueMessage(std::unique_ptr<dapi::message::Message> newMessage);
	std::unique_ptr<dapi::message::Message> getNextMessage();

	bool isConnected() const;
	int messageQueueSize() const;

private:
	sf::UdpSocket udpSocket;
	sf::TcpSocket tcpSocket;
	sf::TcpListener tcpListener;
	sf::SocketSelector socketSelector;
	std::deque<std::unique_ptr<dapi::message::Message>> messageQueue;
	std::mt19937 mt;

	int getRandomInteger(int min, int max)
	{
		std::uniform_int_distribution<int> randomNumber(min, max);
		return randomNumber(mt);
	}

	unsigned short connectionPort;
	bool udpbound;
};
} // namespace DAPI
