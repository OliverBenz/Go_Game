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

bool MoveChecker::isValidMove(Player player, Coord c) {
	return c.x < m_board.size() && c.y < m_board.size()         // Valid board coordinates
	       && m_board.getAt(c) == Board::FieldValue::None       // Field free
	       && !m_seenHashes.contains(hashAfterMove(player, c)); // No game state repeat
}

} // namespace go