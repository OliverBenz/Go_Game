#pragma once

#include "engine/types.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace tengen::engine::gtp {

struct Response {
	bool success{false};
	std::string payload{};
};

std::string toGtpColor(Player player);
std::string toGtpVertex(Coord coord, std::size_t boardSize);
std::optional<Coord> fromGtpVertex(std::string_view vertex, std::size_t boardSize);

std::string buildCommand(std::string_view name, const std::vector<std::string>& arguments = {});
std::optional<Response> parseResponseBlock(std::string_view rawBlock);

std::optional<Move> parseMoveToken(std::string_view token, Player player, std::size_t boardSize);
std::optional<Move> parseMoveResponse(const Response& response, Player player, std::size_t boardSize);

} // namespace tengen::engine::gtp
