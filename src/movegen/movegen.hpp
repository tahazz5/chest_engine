#pragma once
#include "board/attacks.hpp"
#include "board/move_list.hpp"
#include "board/position.hpp"

namespace chess {
// Replaces output. Requires a consistent Position. Never captures the enemy king.
// Ordinary moves may leave our king in check; castles additionally require safe king squares.
// Legality filtering will be introduced with make/unmake in phase 5.
void generatePseudoLegal(const Position& position, const attacks::SlidingAttacks& sliding,
                         MoveList& output) noexcept;
} // namespace chess

namespace chess {
// Temporarily mutates Position and restores it exactly before returning.
void generateLegal(Position& position, const attacks::SlidingAttacks& sliding, MoveList& output) noexcept;
} // namespace chess
