#pragma once
#include "utils/types.hpp"
#include <bit>

namespace chess {
using Bitboard = std::uint64_t;
inline constexpr Bitboard FileA = 0x0101010101010101ULL;
inline constexpr Bitboard FileH = FileA << 7U;
inline constexpr Bitboard Rank1 = 0xFFULL;
inline constexpr Bitboard Rank8 = Rank1 << 56U;
[[nodiscard]] constexpr Bitboard bit(Square square) noexcept {
    return Bitboard{1} << index(square); // precondition excludes shift by 64
}
[[nodiscard]] constexpr Bitboard fileMask(unsigned file) noexcept {
    assert(file < 8); return FileA << file;
}
[[nodiscard]] constexpr Bitboard rankMask(unsigned rank) noexcept {
    assert(rank < 8); return Rank1 << (rank * 8U);
}
[[nodiscard]] constexpr bool contains(Bitboard board, Square square) noexcept { return (board & bit(square)) != 0; }
constexpr void set(Bitboard& board, Square square) noexcept { board |= bit(square); }
constexpr void clear(Bitboard& board, Square square) noexcept { board &= ~bit(square); }
[[nodiscard]] constexpr int count(Bitboard board) noexcept { return std::popcount(board); }
[[nodiscard]] constexpr Square leastSquare(Bitboard board) noexcept {
    return board == 0 ? Square::None : static_cast<Square>(std::countr_zero(board));
}
// Total operation: empty input returns None and stays empty.
constexpr Square popLeastSquare(Bitboard& board) noexcept {
    const auto square = leastSquare(board);
    board &= board - 1; // unsigned wraparound is defined, including zero
    return square;
}
[[nodiscard]] constexpr Bitboard north(Bitboard board) noexcept { return board << 8U; }
[[nodiscard]] constexpr Bitboard south(Bitboard board) noexcept { return board >> 8U; }
[[nodiscard]] constexpr Bitboard east(Bitboard board) noexcept { return (board & ~FileH) << 1U; }
[[nodiscard]] constexpr Bitboard west(Bitboard board) noexcept { return (board & ~FileA) >> 1U; }
[[nodiscard]] constexpr Bitboard northEast(Bitboard board) noexcept { return north(east(board)); }
[[nodiscard]] constexpr Bitboard northWest(Bitboard board) noexcept { return north(west(board)); }
[[nodiscard]] constexpr Bitboard southEast(Bitboard board) noexcept { return south(east(board)); }
[[nodiscard]] constexpr Bitboard southWest(Bitboard board) noexcept { return south(west(board)); }
static_assert(sizeof(Bitboard) == 8);
} // namespace chess
