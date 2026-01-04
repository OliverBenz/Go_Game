#include "network/connection.hpp"

#include <asio/read.hpp>
#include <asio/write.hpp>

#include <array>
#include <chrono>
#include <memory>
#include <string>
#include <utility>

namespace go::network {

// TODO: Proper session id
static std::atomic<std::uint64_t> s_nextSessionId{1};

Connection::Connection(asio::ip::tcp::socket socket, std::size_t clientIndex, Callbacks callbacks)
    : m_socket(std::move(socket)), m_strand(m_socket.get_executor()), m_clientIndex(clientIndex),
      m_sessionId("session-" + std::to_string(s_nextSessionId.fetch_add(1))), m_callbacks(std::move(callbacks)) {
}

void Connection::start() {
	if (m_running.exchange(true)) {
		return;
	}

	m_ioThread = std::thread([this] {
		if (m_callbacks.onConnect) {
			m_callbacks.onConnect(*this);
		}

		startRead();

		auto& ctx = static_cast<asio::io_context&>(m_socket.get_executor().context());
		ctx.run();
	});
}

void Connection::stop() {
	if (!m_running.exchange(false)) {
		return;
	}

	asio::post(m_strand, [this] {
		asio::error_code ec;
		m_socket.shutdown(asio::socket_base::shutdown_both, ec);
		m_socket.close(ec);
	});

	if (m_ioThread.joinable() && m_ioThread.get_id() != std::this_thread::get_id()) {
		m_ioThread.join();
	}
}

void Connection::send(const Message& msg) {
	if (!m_running.load() || msg.size() > MAX_PAYLOAD_BYTES) {
		return;
	}

	asio::post(m_strand, [this, msg] {
		bool idle = m_writeQueue.empty();
		m_writeQueue.push_back(msg);
		if (idle && !m_writeInProgress) {
			startWrite();
		}
	});
}

std::size_t Connection::clientIndex() const {
	return m_clientIndex;
}

const SessionId& Connection::sessionId() const {
	return m_sessionId;
}

void Connection::startWrite() {
	if (!m_running || m_writeQueue.empty()) {
		m_writeInProgress = false;
		return;
	}

	m_writeInProgress = true;

	auto header          = std::make_shared<BasicMessageHeader>();
	header->payload_size = to_network_u32(static_cast<std::uint32_t>(m_writeQueue.front().size()));

	std::array<asio::const_buffer, 2> buffers = {asio::buffer(header.get(), sizeof(BasicMessageHeader)),
	                                             asio::buffer(m_writeQueue.front().data(), m_writeQueue.front().size())};

	asio::async_write(m_socket, buffers, asio::bind_executor(m_strand, [this, header](asio::error_code ec, std::size_t) {
		                  if (ec || !m_running) {
			                  doDisconnect();
			                  return;
		                  }

		                  m_writeQueue.pop_front();
		                  if (!m_writeQueue.empty()) {
			                  startWrite();
		                  } else {
			                  m_writeInProgress = false;
		                  }
	                  }));
}

void Connection::startRead() {
	auto header = std::make_shared<BasicMessageHeader>();

	asio::async_read(m_socket, asio::buffer(header.get(), sizeof(BasicMessageHeader)),
	                 asio::bind_executor(m_strand, [this, header](asio::error_code ec, std::size_t) {
		                 if (ec || !m_running) {
			                 doDisconnect();
			                 return;
		                 }

		                 const auto payloadSize = from_network_u32(header->payload_size);
		                 if (payloadSize > MAX_PAYLOAD_BYTES) {
			                 doDisconnect();
			                 return;
		                 }

		                 if (payloadSize == 0) {
			                 if (m_callbacks.onMessage) {
				                 m_callbacks.onMessage(*this, Message{});
			                 }
			                 startRead();
			                 return;
		                 }

		                 auto payload = std::make_shared<Message>(payloadSize, '\0');
		                 asio::async_read(m_socket, asio::buffer(payload->data(), payload->size()),
		                                  asio::bind_executor(m_strand, [this, payload](asio::error_code ec1, std::size_t) {
			                                  if (ec1 || !m_running) {
				                                  doDisconnect();
				                                  return;
			                                  }

			                                  if (m_callbacks.onMessage) {
				                                  m_callbacks.onMessage(*this, *payload);
			                                  }
			                                  startRead();
		                                  }));
	                 }));
}

void Connection::doDisconnect() {
	if (!m_running.exchange(false)) {
		return;
	}

	asio::error_code ec;
	m_socket.shutdown(asio::socket_base::shutdown_both, ec);
	m_socket.close(ec);

	if (m_callbacks.onDisconnect) {
		m_callbacks.onDisconnect(*this);
	}
}

} // namespace go::network
