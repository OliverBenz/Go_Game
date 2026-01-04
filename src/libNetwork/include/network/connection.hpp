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

	Connection(asio::ip::tcp::socket socket, std::size_t clientIndex, Callbacks callbacks);

	void start();                  //!< Start connection.
	void stop();                   //!< Stop connection.
	void send(const Message& msg); //!< Send message to client.

	std::size_t clientIndex() const;    //!< Get client index of this connection.
	const SessionId& sessionId() const; //!< Get client session id of this connection.

private:
	void startRead();    //!< Prime async read and dispatch messages.
	void startWrite();   //!< Prime async write for new messages.
	void doDisconnect(); //!< Internal cleanup.

private:
	std::thread m_ioThread;                       //!< ASIO input output thread.
	std::atomic<bool> m_running{false};           //!< Connection (and io thread) running.
	asio::ip::tcp::socket m_socket;               //!< Client socket.
	asio::strand<asio::any_io_executor> m_strand; //!< IO with different threads.

	// TODO: Dont store. its just internal id for server. Not used here ever - only passed to server.
	// Also: Session should be uniquely identified by sessionId. No need for another index.
	std::size_t m_clientIndex;
	SessionId m_sessionId; //!< Unique identifier for user session.
	Callbacks m_callbacks; //!< Used to signal to the parent.

	std::deque<Message> m_writeQueue;
	bool m_writeInProgress{false};
};

} // namespace go::network
