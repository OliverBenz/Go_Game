#pragma once

namespace go::network {

//! Transportation primitive. Handles read/write from a single client connection.
class Connection {
public:
    struct Callbacks {
        std::function<void(Connection&)> onConnect;
        std::function<void(Connection&, const Message&)> onMessage;
        std::function<void(Connection&)> onDisconnect;
    };

    Connection(asio::ip::tcp::socket socket, int clientIndex, Callbacks callbacks);

    void start();                  //!< Start connection.
    void stop();                   //!< Stop connection.
    void send(const Message& msg); //!< Send message to client.

    int clientIndex() const; //!< Get client index of this connection.

private:
    void readLoop();                   //!< Blocking read.
    bool readOneMessage(Message& out); //!< Read single message.
    void doDisconnect();               //!< Internal cleanup.

private:
    asio::ip::tcp::socket m_socket;
    int m_clientIndex;

    std::thread m_ioThread;
    std::mutex m_writeMutex;
    std::deque<Message> m_writeQueue;

    std::atomic<bool> m_running{false};
};

}