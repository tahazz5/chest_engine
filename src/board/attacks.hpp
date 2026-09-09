#pragma once

#include "board/bitboard.hpp"
#include <array>
#include <cstddef>
#include <memory>

namespace chess {
class Position;

namespace attacks {
namespace detail {
struct Step { int file; int rank; };
inline constexpr std::array<Step, 8> KnightSteps{{
    {1, 2}, {2, 1}, {2, -1}, {1, -2}, {-1, -2}, {-2, -1}, {-2, 1}, {-1, 2}}};
inline constexpr std::array<Step, 8> KingSteps{{
    {0, 1}, {1, 1}, {1, 0}, {1, -1}, {0, -1}, {-1, -1}, {-1, 0}, {-1, 1}}};
template<std::size_t N>
constexpr std::array<Bitboard, 64> makeLeaperTable(const std::array<Step, N>& steps) noexcept {
    std::array<Bitboard, 64> table{};
    for (int s = 0; s < 64; ++s) {
        for (const auto step : steps) {
            const auto target = makeSquare(s % 8 + step.file, s / 8 + step.rank);
            if (isValid(target)) table[static_cast<unsigned>(s)] |= bit(target);
        }
    }
    return table;
}
inline constexpr auto KnightTable = makeLeaperTable(KnightSteps);
inline constexpr auto KingTable = makeLeaperTable(KingSteps);
inline constexpr std::array PawnTables{
    makeLeaperTable(std::array<Step, 2>{{{-1, 1}, {1, 1}}}),
    makeLeaperTable(std::array<Step, 2>{{{-1, -1}, {1, -1}}})};
} // namespace detail

// Attacks are geometric: include friendly-occupied targets, exclude the origin.
[[nodiscard]] constexpr Bitboard knight(Square square) noexcept { return detail::KnightTable[index(square)]; }
[[nodiscard]] constexpr Bitboard king(Square square) noexcept { return detail::KingTable[index(square)]; }
[[nodiscard]] constexpr Bitboard pawn(Color color, Square square) noexcept {
    assert(isValid(color));
    return detail::PawnTables[static_cast<unsigned>(color)][index(square)];
}

// Include the first blocker, then stop. Occupancy may include the origin.
[[nodiscard]] Bitboard rookRays(Square square, Bitboard occupancy) noexcept;
[[nodiscard]] Bitboard bishopRays(Square square, Bitboard occupancy) noexcept;
[[nodiscard]] inline Bitboard queenRays(Square square, Bitboard occupancy) noexcept {
    return rookRays(square, occupancy) | bishopRays(square, occupancy);
}
// Only these occupancy bits affect a sliding attack; terminal edge squares are omitted.
[[nodiscard]] Bitboard rookRelevantMask(Square square) noexcept;
[[nodiscard]] Bitboard bishopRelevantMask(Square square) noexcept;

// Software equivalent of PEXT: pack bits selected by mask into consecutive low bits.
[[nodiscard]] constexpr std::uint64_t extractBits(Bitboard occupancy, Bitboard mask) noexcept {
    std::uint64_t result = 0, outputBit = 1;
    while (mask != 0) {
        const Bitboard lowest = mask & (~mask + 1);
        if ((occupancy & lowest) != 0) result |= outputBit;
        mask &= mask - 1;
        outputBit <<= 1U;
    }
    return result;
}
[[nodiscard]] bool hardwarePextAvailable() noexcept;

enum class SlidingBackend : std::uint8_t { Rays, PortableTable, PextTable, Auto };

// One owner, constructed before search; immutable thereafter, share by const reference.
class SlidingAttacks {
public:
    explicit SlidingAttacks(SlidingBackend requested = SlidingBackend::Rays);
    ~SlidingAttacks();
    SlidingAttacks(const SlidingAttacks&) = delete;
    SlidingAttacks& operator=(const SlidingAttacks&) = delete;
    SlidingAttacks(SlidingAttacks&&) = delete;
    SlidingAttacks& operator=(SlidingAttacks&&) = delete;

    [[nodiscard]] SlidingBackend backend() const noexcept { return backend_; }
    [[nodiscard]] Bitboard rook(Square square, Bitboard occupancy) const noexcept;
    [[nodiscard]] Bitboard bishop(Square square, Bitboard occupancy) const noexcept;
    [[nodiscard]] Bitboard queen(Square square, Bitboard occupancy) const noexcept {
        return rook(square, occupancy) | bishop(square, occupancy);
    }
    [[nodiscard]] std::size_t tableBytes() const noexcept;
    inline static constexpr std::size_t RookEntries = 102400;
    inline static constexpr std::size_t BishopEntries = 5248;
private:
    struct Tables;
    std::unique_ptr<const Tables> tables_;
    SlidingBackend backend_;
};

[[nodiscard]] Bitboard attackersTo(const Position& position, Square target, Color byColor,
                                  const SlidingAttacks& sliding) noexcept;
[[nodiscard]] bool isSquareAttacked(const Position& position, Square target, Color byColor,
                                    const SlidingAttacks& sliding) noexcept;
[[nodiscard]] bool inCheck(const Position& position, Color color, const SlidingAttacks& sliding) noexcept;
} // namespace attacks
} // namespace chess
