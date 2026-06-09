#include "engine/gtp.hpp"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <charconv>

namespace tengen::engine::gtp {
namespace {

constexpr std::string_view GTP_COLUMNS = "ABCDEFGHJKLMNOPQRSTUVWXYZ";
constexpr unsigned GTP_COLUMN_COUNT    = 25u;

std::string trimCopy(std::string_view input) {
	const auto first = input.find_first_not_of(" \t\r\n");
	if (first == std::string_view::npos) {
		return {};
	}

	const auto last = input.find_last_not_of(" \t\r\n");
	return std::string{input.substr(first, last - first + 1)};
}

std::string uppercaseCopy(std::string_view input) {
	std::string out;
	out.reserve(input.size());
	for (const char ch: input) {
		out.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(ch))));
	}
	return out;
}

std::string encodeColumn(unsigned x) {
	std::string out;
	do {
		const auto idx = x % GTP_COLUMN_COUNT;
		out.push_back(GTP_COLUMNS[idx]);
		if (x < GTP_COLUMN_COUNT) {
			break;
		}
		x = x / GTP_COLUMN_COUNT - 1u;
	} while (true);

	std::reverse(out.begin(), out.end());
	return out;
}

std::optional<unsigned> decodeColumn(std::string_view value) {
	if (value.empty()) {
		return std::nullopt;
	}

	unsigned result = 0u;
	for (const char raw: value) {
		const char ch = static_cast<char>(std::toupper(static_cast<unsigned char>(raw)));
		const auto idx = GTP_COLUMNS.find(ch);
		if (idx == std::string_view::npos) {
			return std::nullopt;
		}
		result = result * GTP_COLUMN_COUNT + static_cast<unsigned>(idx) + 1u;
	}

	return result - 1u;
}

std::optional<unsigned> parseUnsigned(std::string_view text) {
	unsigned value = 0u;
	const auto first = text.data();
	const auto last  = text.data() + text.size();
	const auto [ptr, ec] = std::from_chars(first, last, value);
	if (ec != std::errc{} || ptr != last) {
		return std::nullopt;
	}
	return value;
}

} // namespace

std::string toGtpColor(const Player player) {
	assert(player == Player::Black || player == Player::White);
	return player == Player::Black ? "B" : "W";
}

std::string toGtpVertex(const Coord coord, const std::size_t boardSize) {
	assert(coord.x < boardSize);
	assert(coord.y < boardSize);
	return encodeColumn(coord.x) + std::to_string(boardSize - coord.y);
}

std::optional<Coord> fromGtpVertex(const std::string_view vertex, const std::size_t boardSize) {
	const auto trimmed = trimCopy(vertex);
	if (trimmed.empty()) {
		return std::nullopt;
	}

	std::size_t split = 0u;
	while (split < trimmed.size() && std::isalpha(static_cast<unsigned char>(trimmed[split])) != 0) {
		++split;
	}

	if (split == 0u || split == trimmed.size()) {
		return std::nullopt;
	}

	const auto column = decodeColumn(std::string_view{trimmed}.substr(0u, split));
	const auto row    = parseUnsigned(std::string_view{trimmed}.substr(split));
	if (!column || !row || *column >= boardSize || *row == 0u || *row > boardSize) {
		return std::nullopt;
	}

	return Coord{*column, static_cast<unsigned>(boardSize - *row)};
}

std::string buildCommand(const std::string_view name, const std::vector<std::string>& arguments) {
	std::string command{name};
	for (const auto& arg: arguments) {
		command += ' ';
		command += arg;
	}
	command += '\n';
	return command;
}

std::optional<Response> parseResponseBlock(const std::string_view rawBlock) {
	const auto trimmed = trimCopy(rawBlock);
	if (trimmed.empty()) {
		return std::nullopt;
	}

	if (trimmed.front() != '=' && trimmed.front() != '?') {
		return std::nullopt;
	}

	std::string payload = trimCopy(std::string_view{trimmed}.substr(1u));
	if (!payload.empty()) {
		const auto tokenEnd = payload.find_first_of(" \t\r\n");
		const auto token    = payload.substr(0u, tokenEnd);
		if (!token.empty() && std::all_of(token.begin(), token.end(), [](const char ch) { return std::isdigit(static_cast<unsigned char>(ch)) != 0; })) {
			if (tokenEnd == std::string::npos) {
				payload.clear();
			} else {
				payload = trimCopy(std::string_view{payload}.substr(tokenEnd));
			}
		}
	}

	return Response{
	        .success = trimmed.front() == '=',
	        .payload = std::move(payload),
	};
}

std::optional<Move> parseMoveToken(const std::string_view token, const Player player, const std::size_t boardSize) {
	const auto normalized = uppercaseCopy(trimCopy(token));
	if (normalized.empty()) {
		return std::nullopt;
	}

	if (normalized == "PASS") {
		return Move{.kind = MoveKind::Pass, .player = player, .coord = std::nullopt};
	}
	if (normalized == "RESIGN") {
		return Move{.kind = MoveKind::Resign, .player = player, .coord = std::nullopt};
	}

	const auto coord = fromGtpVertex(normalized, boardSize);
	if (!coord) {
		return std::nullopt;
	}

	return Move{.kind = MoveKind::Place, .player = player, .coord = coord};
}

std::optional<Move> parseMoveResponse(const Response& response, const Player player, const std::size_t boardSize) {
	if (!response.success) {
		return std::nullopt;
	}

	const auto payload = trimCopy(response.payload);
	if (payload.empty()) {
		return std::nullopt;
	}

	const auto firstBreak = payload.find_first_of(" \t\r\n");
	const auto firstToken = firstBreak == std::string::npos ? payload : payload.substr(0u, firstBreak);
	if (uppercaseCopy(firstToken) == "PLAY") {
		if (firstBreak == std::string::npos) {
			return std::nullopt;
		}
		return parseMoveToken(payload.substr(firstBreak + 1u), player, boardSize);
	}

	return parseMoveToken(payload, player, boardSize);
}

} // namespace tengen::engine::gtp
