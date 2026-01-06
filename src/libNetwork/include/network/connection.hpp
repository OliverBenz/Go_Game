#pragma once

#include "network/protocol.hpp"

#include <asio.hpp>

#include <atomic>
#include <deque>
#include <functional>
#include <memory>
#include <thread>
#include <variant>

namespace go::network {

//! Transportation primitive. Handles read/write from a single client connection.
class Connection {
public:
	struct Callbacks {
		std::function<void(Connection&)> onConnect;
		std::function<void(Connection&, const Message&)> onMessage;
		std::function<void(Connection&)> onDisconnect;
	};

	Connection(asio::ip::tcp::socket socket, ConnectionId connectionId, Callbacks callbacks);

	void start();                  //!< Start connection.
	void stop();                   //!< Stop connection.
	void send(const Message& msg); //!< Send message to client.

	ConnectionId connectionId() const; //!< Get the identifier of this connection.

private:
	void startRead();    //!< Prime async read and dispatch messages.
	void startWrite();   //!< Prime async write for new messages.
	void doDisconnect(); //!< Internal cleanup.

private:
	std::thread m_ioThread;                       //!< ASIO input output thread.
	std::atomic<bool> m_running{false};           //!< Connection (and io thread) running.
	asio::ip::tcp::socket m_socket;               //!< Client socket.
	asio::strand<asio::any_io_executor> m_strand; //!< IO with different threads.

	ConnectionId m_connectionId; //!< Unique identifier for user session.
	Callbacks m_callbacks;       //!< Used to signal to the parent.

	std::deque<Message> m_writeQueue;
	bool m_writeInProgress{false};
};

} // namespace go::network
