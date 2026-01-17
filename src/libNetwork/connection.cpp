#include "connection.hpp"

#include <asio/read.hpp>
#include <asio/write.hpp>

#include <array>
#include <memory>
#include <string>
#include <utility>

namespace go::network {

Connection::Connection(asio::ip::tcp::socket socket, ConnectionId connectionId, Callbacks callbacks)
    : m_socket(std::move(socket)), m_strand(m_socket.get_executor()), m_connectionId(connectionId), m_callbacks(std::move(callbacks)) {
}

Connection::~Connection() {
	stop();
}

void Connection::start() {
	if (m_running.exchange(true)) {
		return;
	}

	if (m_callbacks.onConnect) {
		m_callbacks.onConnect(*this);
	}

	startRead();
}

void Connection::stop() {
	if (m_running.exchange(false)) {
		asio::error_code ec;
		m_socket.shutdown(asio::socket_base::shutdown_both, ec);
		m_socket.close(ec);
	}
}

void Connection::send(const Message& msg) {
	if (!m_running.load() || msg.size() > MAX_PAYLOAD_BYTES) {
		return;
	}

	auto self = shared_from_this();
	asio::post(m_strand, [self, msg] {
		bool idle = self->m_writeQueue.empty();
		self->m_writeQueue.push_back(msg);
		if (idle && !self->m_writeInProgress) {
			self->startWrite();
		}
	});
}

ConnectionId Connection::connectionId() const {
	return m_connectionId;
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

	auto self = shared_from_this();
	asio::async_write(m_socket, buffers, asio::bind_executor(m_strand, [self, header](asio::error_code ec, std::size_t) {
		                  if (ec || !self->m_running) {
			                  self->doDisconnect();
			                  return;
		                  }

		                  self->m_writeQueue.pop_front();
		                  if (!self->m_writeQueue.empty()) {
			                  self->startWrite();
		                  } else {
			                  self->m_writeInProgress = false;
		                  }
	                  }));
}

void Connection::startRead() {
	auto header = std::make_shared<BasicMessageHeader>();

	auto self = shared_from_this();
	asio::async_read(m_socket, asio::buffer(header.get(), sizeof(BasicMessageHeader)),
	                 asio::bind_executor(m_strand, [self, header](asio::error_code ec, std::size_t) {
		                 if (ec || !self->m_running) {
			                 self->doDisconnect();
			                 return;
		                 }

		                 const auto payloadSize = from_network_u32(header->payload_size);
		                 if (payloadSize > MAX_PAYLOAD_BYTES) {
			                 self->doDisconnect();
			                 return;
		                 }

		                 if (payloadSize == 0) {
			                 if (self->m_callbacks.onMessage) {
				                 self->m_callbacks.onMessage(*self, Message{});
			                 }
			                 self->startRead();
			                 return;
		                 }

		                 auto payload = std::make_shared<Message>(payloadSize, '\0');
		                 asio::async_read(self->m_socket, asio::buffer(payload->data(), payload->size()),
		                                  asio::bind_executor(self->m_strand, [self, payload](asio::error_code ec1, std::size_t) {
			                                  if (ec1 || !self->m_running) {
				                                  self->doDisconnect();
				                                  return;
			                                  }

			                                  if (self->m_callbacks.onMessage) {
				                                  self->m_callbacks.onMessage(*self, *payload);
			                                  }
			                                  self->startRead();
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
