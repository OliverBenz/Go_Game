#include <asio.hpp>
#include <string>
#include <iostream>

int main() {
    asio::io_context io_context;
    asio::ip::tcp::resolver resolver(io_context);
    asio::ip::tcp::socket socket(io_context);

    auto endpoints = resolver.resolve("localhost", "12345");
    asio::connect(socket, endpoints);

    std::string message{"12345678"};
    asio::write(socket, asio::buffer(message));

    std::cout << "Sent message: " << message << "\n";
    return 0;
}