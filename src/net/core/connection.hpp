#pragma once

#include "network/core/protocol.hpp"

#include <asio.hpp>

#include <atomic>
#include <deque>
#include <functional>
#include <memory>

namespace go::network::core {

//! Transportation primitive. Handles read/write from a single client connection.
//! \note Internals are async and run on the server IO thread.
//!       We use shared_from_this() so any in-flight async op keeps the Connection alive.
class Connection : public std::enable_shared_from_this<Connection> {
public:
	struct Callbacks {
		std::function<void(Connection&)> onConnect;
		std::function<void(Connection&, const Message&)> onMessage;
		std::function<void(Connection&)> onDisconnect;
	};

	Connection(asio::ip::tcp::socket socket, ConnectionId connectionId, Callbacks callbacks);
	~Connection();

	void start();                  //!< Start connection: begins async read loop and triggers onConnect.
	void stop();                   //!< Stop connection: closes the socket and cancels IO (best-effort).
	void send(const Message& msg); //!< Send message to client. Safe to call from any thread.

	ConnectionId connectionId() const; //!< Get the identifier of this connection.

private:
	void startRead();    //!< Prime async read and dispatch messages.
	void startWrite();   //!< Prime async write for new messages.
	void doDisconnect(); //!< Internal cleanup.

private:
	std::atomic<bool> m_running{false};           //!< Connection (and io thread) running.
	asio::ip::tcp::socket m_socket;               //!< Client socket.
	asio::strand<asio::any_io_executor> m_strand; //!< IO with different threads.

	ConnectionId m_connectionId; //!< Unique identifier for user session.
	Callbacks m_callbacks;       //!< Used to signal to the parent.

	std::deque<Message> m_writeQueue;
	bool m_writeInProgress{false};
};

} // namespace go::network::core
