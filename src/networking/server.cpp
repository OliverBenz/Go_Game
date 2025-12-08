#include <asio.hpp>
#include <iostream>
#include <string>

int main() {
	asio::io_context io_context;
	asio::ip::tcp::acceptor acceptor(io_context, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), 12345));

	std::cout << "Server listening to port 12345.\n";

	asio::ip::tcp::socket socket(io_context);
	acceptor.accept(socket);

	while (true) {
		char data[8] = {};
		asio::error_code error;

		// TODO: char array size same as read_some size? or with '\0' at end?
		std::size_t len = socket.read_some(asio::buffer(data, 8), error);

		if (error == asio::error::eof)
			break;
		else if (error)
			throw asio::system_error(error);

		std::cout << "Received message: " << std::string(data, len) << "\n";
	}

	std::cout << "Shutting Down\n";
	return 0;
}