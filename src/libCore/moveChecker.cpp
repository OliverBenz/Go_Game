#include "core/moveChecker.hpp"

#include "core/game.hpp"
#include "core/moveChecker.hpp"

#include <array>
#include <cassert>

namespace go {

static constexpr std::array<int, 4> kDx{1, -1, 0, 0};
static constexpr std::array<int, 4> kDy{0, 0, 1, -1};

static bool inBounds(const Board& board, Coord c) {
	return c.x < board.size() && c.y < board.size();
}

static std::size_t groupAnalysis(const Board& board, const Coord startCoord, const Player player, std::vector<Coord>& group,
                                 const std::optional<Coord> pretendStone, const std::optional<Coord> blockedLiberty) {
	const auto boardSize = board.size();

	auto isPlayerStone = [&](Coord c) {
		if (pretendStone && pretendStone->x == c.x && pretendStone->y == c.y)
			return true;
		return board.getAt(c) == toBoardValue(player);
	};

	std::vector<std::vector<bool>> visited(boardSize, std::vector<bool>(boardSize, false));
	std::vector<std::vector<bool>> libertyVisited(boardSize, std::vector<bool>(boardSize, false));

	std::vector<Coord> stack;
	if (isPlayerStone(startCoord)) {
		stack.push_back(startCoord);
		group.push_back(startCoord);
	} else {
		return 0;
	}

	std::size_t liberties = 0;
	while (!stack.empty()) {
		const auto c = stack.back();
		stack.pop_back();
		if (visited[c.x][c.y])
			continue;
		visited[c.x][c.y] = true;

		for (std::size_t i = 0; i < kDx.size(); ++i) {
			const int nx = static_cast<int>(c.x) + kDx[i];
			const int ny = static_cast<int>(c.y) + kDy[i];
			if (nx < 0 || ny < 0 || nx >= static_cast<int>(boardSize) || ny >= static_cast<int>(boardSize))
				continue;

			const Coord neighbor{static_cast<Id>(nx), static_cast<Id>(ny)};
			if (isPlayerStone(neighbor)) {
				if (!visited[neighbor.x][neighbor.y]) {
					stack.push_back(neighbor);
					group.push_back(neighbor);
				}
				continue;
			}

			if (board.getAt(neighbor) == Board::Value::Empty &&
			    (!blockedLiberty || (blockedLiberty->x != neighbor.x || blockedLiberty->y != neighbor.y)) &&
			    !libertyVisited[neighbor.x][neighbor.y]) {
				libertyVisited[neighbor.x][neighbor.y] = true;
				++liberties;
			}
		}
	}

	return liberties;
}

std::size_t computeGroupLiberties(const Board& board, Coord startCoord, Player player) {
	if (!inBounds(board, startCoord))
		return 0;
	std::vector<Coord> group;
	return groupAnalysis(board, startCoord, player, group, startCoord, std::nullopt);
}

static bool wouldCapture(const Board& board, Coord c, Player player) {
	const auto boardSize = board.size();

	std::vector<std::vector<bool>> visited(boardSize, std::vector<bool>(boardSize, false));
	std::vector<Coord> group;
	const auto enemy = opponent(player);

	for (std::size_t i = 0; i < kDx.size(); ++i) {
		const int nx = static_cast<int>(c.x) + kDx[i];
		const int ny = static_cast<int>(c.y) + kDy[i];
		if (nx < 0 || ny < 0 || nx >= static_cast<int>(boardSize) || ny >= static_cast<int>(boardSize))
			continue;

		const Coord neighbor{static_cast<Id>(nx), static_cast<Id>(ny)};
		if (board.getAt(neighbor) != toBoardValue(enemy) || visited[neighbor.x][neighbor.y])
			continue;

		group.clear();
		const auto liberties = groupAnalysis(board, neighbor, enemy, group, std::nullopt, c);
		for (const auto stone: group) {
			visited[stone.x][stone.y] = true;
		}
		if (liberties == 0) {
			return true;
		}
	}

	return false;
}

bool isSuicide(const Board& board, Player player, Coord c) {
	// Direct liberties after placing the stone (ignores captures).
	if (computeGroupLiberties(board, c, player) > 0) {
		return false;
	}

	// Capturing neighbours can save the move.
	return !wouldCapture(board, c, player);
}

static Position simulatePosition(const Position& start, Coord move, Player player, IZobristHash& hasher) {
	assert(start.board.isFree(move));

	const auto boardSize = start.board.size();
	const auto enemy     = opponent(player);

	std::vector<std::vector<bool>> visited(boardSize, std::vector<bool>(boardSize, false));
	std::vector<std::vector<bool>> captured(boardSize, std::vector<bool>(boardSize, false));
	std::vector<Coord> group;

	for (std::size_t i = 0; i < kDx.size(); ++i) {
		const int nx = static_cast<int>(move.x) + kDx[i];
		const int ny = static_cast<int>(move.y) + kDy[i];
		if (nx < 0 || ny < 0 || nx >= static_cast<int>(boardSize) || ny >= static_cast<int>(boardSize))
			continue;

		const Coord neighbor{static_cast<Id>(nx), static_cast<Id>(ny)};
		if (start.board.getAt(neighbor) != toBoardValue(enemy) || visited[neighbor.x][neighbor.y])
			continue;

		group.clear();
		const auto liberties = groupAnalysis(start.board, neighbor, enemy, group, std::nullopt, move);
		for (const auto stone: group) {
			visited[stone.x][stone.y] = true;
		}
		if (liberties == 0) {
			for (const auto stone: group) {
				captured[stone.x][stone.y] = true;
			}
		}
	}

	Board nextBoard{boardSize};
	for (Id x = 0; x < boardSize; ++x) {
		for (Id y = 0; y < boardSize; ++y) {
			const Coord pos{x, y};
			if (captured[x][y] || (pos.x == move.x && pos.y == move.y))
				continue;

			const auto value = start.board.getAt(pos);
			if (value != Board::Value::Empty) {
				nextBoard.setAt(pos, value);
			}
		}
	}
	nextBoard.setAt(move, toBoardValue(player));

	uint64_t nextHash = start.hash;
	nextHash ^= hasher.stone(move, player);
	for (Id x = 0; x < boardSize; ++x) {
		for (Id y = 0; y < boardSize; ++y) {
			if (captured[x][y]) {
				nextHash ^= hasher.stone({x, y}, enemy);
			}
		}
	}
	nextHash ^= hasher.togglePlayer();

	Position next      = start;
	next.board         = std::move(nextBoard);
	next.currentPlayer = opponent(player);
	next.hash          = nextHash;
	return next;
}

bool isValidMove(const Board& board, Player player, Coord c) {
	if (!inBounds(board, c) || board.getAt(c) != Board::Value::Empty)
		return false;

	return !isSuicide(board, player, c);
}

bool isNextPositionLegal(const Position& current, Player player, Coord c, IZobristHash& hasher, const std::unordered_set<uint64_t>& history,
                         Position& out) {
	if (!isValidMove(current.board, player, c))
		return false;

	Position simulated = simulatePosition(current, c, player, hasher);
	if (history.contains(simulated.hash))
		return false;

	out = std::move(simulated);
	return true;
}

} // namespace go
