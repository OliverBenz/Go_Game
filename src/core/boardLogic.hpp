#pragma once

enum Player {
    black = 1, 
    white = -1,
}

// TODO: This should be optimized to use 2Bits per site on the field. (B, W, None, ...)
template <unsigned size>
class BoardLogic {
public:
    bool addStone(unsigned x, unsigned y, Player player) {
        board[x * size + y] = static_cast<char>(player);
    }

private:
    std::array<char, size*size> board{};
};
