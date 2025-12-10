#include "core/moveChecker.hpp"

#include "zobristHash.hpp"

#include <cassert>
#include <stack>

namespace go {

MoveChecker::~MoveChecker() = default;
MoveChecker::MoveChecker(const Board& board) : m_board(board) {
	switch (board.size()) {
	case 9u:
		m_hasher = std::make_unique<ZobristHash<9u>>();
		break;
	case 13u:
		m_hasher = std::make_unique<ZobristHash<13u>>();
		break;
	case 19u:
		m_hasher = std::make_unique<ZobristHash<19u>>();
		break;
	default:
		assert(false);
		break;
	}
}

uint64_t MoveChecker::hashAfterMove(Player player, Coord c) {
	auto hash = m_hasher->lookAhead(c, player);

	// TODO: Remove captured stones after move

	return hash;
}


std::size_t MoveChecker::computeGroupLiberties(const Coord& startCoord, const Player player) {
	const auto N           = m_board.size();
	const auto friendColor = player == Player::White ? Board::FieldValue::White : Board::FieldValue::Black;

	std::vector<std::vector<bool>> visited(N, std::vector<bool>(N, false));

	std::stack<Coord> stack;
	stack.push(startCoord);

	// TODO: Ensure startCoord is counted as a player stone. Its not on board
	std::size_t liberties = 0;
	while (!stack.empty()) {
		const auto c = stack.top();
		stack.pop();
		if (visited[c.x][c.y])
			continue;
		visited[c.x][c.y] = true;

		// Nearest neighbours
		static constexpr std::array<int, 4> dx = {1, -1, 0, 0};
		static constexpr std::array<int, 4> dy = {0, 0, 1, -1};
		for (std::size_t i = 0; i != dx.size(); ++i) {
			Coord cN = {c.x + dx[i], c.y + dy[i]};
			if (cN.x >= N || cN.y >= N) {
				continue;
			}

			const auto value = m_board.getAt(cN);
			if (value == Board::FieldValue::None) {
				++liberties;
			} else if (value == friendColor) {
				stack.push(c);
			}
		}
	}

	return liberties;
}

// A suicide is a move where the number of free liberties of the connected group becomes zero after the move and the
// move does not open up liberties (kill surrounding enemy).
// We move from simple to expensive checks.
bool MoveChecker::isSuicide(Player player, Coord c) {
	// Move safe if connected group has free liberties
	if (computeGroupLiberties(c, player) == 0) {
		// TODO: Check if move kills enemy group
		// Caputure: Move reduces number of any enemy group liberties to zero and is not repeated game state.
		return true;
	}
	return false;
}

bool MoveChecker::isValidMove(Player player, Coord c) {
	return c.x < m_board.size() && c.y < m_board.size()        // Valid board coordinates
	       && m_board.getAt(c) == Board::FieldValue::None      // Field free
	       && !m_seenHashes.contains(hashAfterMove(player, c)) // No game state repeat
	       && !isSuicide(player, c);
}

} // namespace go