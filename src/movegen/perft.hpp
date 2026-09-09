#pragma once
#include "movegen/movegen.hpp"
namespace chess {
[[nodiscard]] inline std::uint64_t perft(Position& p, int depth, const attacks::SlidingAttacks& sliding) noexcept {
    assert(depth >= 0);
    if (depth == 0) return 1;
    MoveList moves;
    generateLegal(p, sliding, moves);
    if (depth == 1) return moves.size();
    std::uint64_t nodes = 0;
    for (const auto move : moves) {
        const auto undo = p.makeMove(move);
        nodes += perft(p, depth - 1, sliding);
        p.unmakeMove(move, undo);
    }
    return nodes;
}
} // namespace chess
