
//! Core game setup.
class Game {
public:
    Game(IRenderer& renderer, INetworkManager& network);
    void run();


private:
    Board m_board;
    IRenderer& m_renderer;
    INetworkManager& m_network;  // Abstract to one-day allow simple extension (e.g. go-online compatibility)
};
