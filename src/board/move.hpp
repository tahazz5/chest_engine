#pragma once
#include "utils/types.hpp"
#include <type_traits>

namespace chess {
// Bits 12..15: bit 2 denotes capture, bit 3 denotes promotion.
// For promotions, bits 0..1 select N, B, R, Q.
enum class MoveFlag : std::uint8_t {
    Quiet = 0, DoublePawnPush = 1, KingCastle = 2, QueenCastle = 3,
    Capture = 4, EnPassant = 5,
    PromoteKnight = 8, PromoteBishop = 9, PromoteRook = 10, PromoteQueen = 11,
    CapturePromoteKnight = 12, CapturePromoteBishop = 13,
    CapturePromoteRook = 14, CapturePromoteQueen = 15
};
[[nodiscard]] constexpr bool isValid(MoveFlag flag) noexcept {
    const auto code = static_cast<unsigned>(flag);
    return code < 16 && code != 6 && code != 7;
}

class Move {
public:
    constexpr Move() noexcept = default; // zero is the no-move sentinel
    constexpr Move(Square from, Square to, MoveFlag flag = MoveFlag::Quiet) noexcept
        : bits_(encode(from, to, flag)) {}

    [[nodiscard]] constexpr bool isNone() const noexcept { return bits_ == 0; }
    [[nodiscard]] constexpr Square from() const noexcept {
        return isNone() ? Square::None : static_cast<Square>(bits_ & 63U);
    }
    [[nodiscard]] constexpr Square to() const noexcept {
        return isNone() ? Square::None : static_cast<Square>((bits_ >> 6U) & 63U);
    }
    [[nodiscard]] constexpr MoveFlag flag() const noexcept { return static_cast<MoveFlag>(bits_ >> 12U); }
    [[nodiscard]] constexpr bool isCapture() const noexcept { return (bits_ & 0x4000U) != 0; }
    [[nodiscard]] constexpr bool isPromotion() const noexcept { return (bits_ & 0x8000U) != 0; }
    [[nodiscard]] constexpr bool isEnPassant() const noexcept { return flag() == MoveFlag::EnPassant; }
    [[nodiscard]] constexpr bool isCastling() const noexcept {
        return flag() == MoveFlag::KingCastle || flag() == MoveFlag::QueenCastle;
    }
    [[nodiscard]] constexpr bool isDoublePawnPush() const noexcept { return flag() == MoveFlag::DoublePawnPush; }
    [[nodiscard]] constexpr PieceType promotedPiece() const noexcept {
        return isPromotion() ? static_cast<PieceType>(2U + ((bits_ >> 12U) & 3U)) : PieceType::None;
    }
    [[nodiscard]] constexpr std::uint16_t raw() const noexcept { return bits_; }
    [[nodiscard]] static constexpr std::optional<Move> fromRaw(std::uint16_t raw) noexcept {
        if (raw == 0) return Move{};
        if (!isValid(static_cast<MoveFlag>(raw >> 12U)) || (raw & 63U) == ((raw >> 6U) & 63U))
            return std::nullopt;
        Move move;
        move.bits_ = raw;
        return move;
    }
    friend constexpr bool operator==(Move, Move) noexcept = default;
private:
    static constexpr std::uint16_t encode(Square from, Square to, MoveFlag flag) noexcept {
        assert(isValid(from) && isValid(to) && from != to && isValid(flag));
        return static_cast<std::uint16_t>(index(from) | (index(to) << 6U) |
                                          (static_cast<unsigned>(flag) << 12U));
    }
    std::uint16_t bits_ = 0;
};
static_assert(sizeof(Move) == 2);
static_assert(std::is_trivially_copyable_v<Move> && std::is_standard_layout_v<Move>);
} // namespace chess
