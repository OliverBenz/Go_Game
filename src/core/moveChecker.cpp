#include "core/moveChecker.hpp"

#include "zobristHash.hpp"

#include <cassert>

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


// A suicide is a move where the number of free liberties of the connected group becomes zero after the move and the
// move does not open up liberties (kill surrounding enemy).
// We move from simple to expensive checks.
bool MoveChecker::isSuicide(Player player, Coord c) {
	const auto ENEMY = player == Player::Black ? Board::FieldValue::White : Board::FieldValue::Black;
	const std::array<Coord, 4> nnCoords{{c.x - 1, c.y}, {c.x + 1, c.y}, {c.x, c.y - 1}, {c.x, c.y + 1}};

	// Move always safe if one liberty
	unsigned liberties = 0;
	for (const auto& c: nnCoords) {
		liberties += static_cast<unsigned>(m_board.isFree(c));
	}
	if (liberties > 0) {
		return false;
	}

	// Move safe if connected group has free liberties



	// No liberties after move: 
	// Move safe if the connected group kills the surrounding enemy group:
	//   Count liberties of neighbour chain
	//   return numberLiberties == 1 // Last libery closed -> Kill group
}

bool MoveChecker::isValidMove(Player player, Coord c) {


	return c.x < m_board.size() && c.y < m_board.size()        // Valid board coordinates
	       && m_board.getAt(c) == Board::FieldValue::None      // Field free
	       && !m_seenHashes.contains(hashAfterMove(player, c)) // No game state repeat
	       && !isSuicide(player, c);
}

} // namespace go