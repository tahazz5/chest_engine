#pragma once
#include "movegen/movegen.hpp"
#include <iosfwd>
#include <string>
namespace chess {
enum class PlayMode { White, Black, Local };
[[nodiscard]] std::string moveText(Move move);
[[nodiscard]] Move chooseSimpleMove(Position& position, const attacks::SlidingAttacks& sliding);
int play(std::istream& input, std::ostream& output, PlayMode mode = PlayMode::White, Position position = Position{});
} // namespace chess
