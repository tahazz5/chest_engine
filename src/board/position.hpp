#pragma once

#include "board/bitboard.hpp"
#include "board/move.hpp"
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>

namespace chess {

enum class CastlingRights : std::uint8_t {
    None = 0, WhiteKingSide = 1, WhiteQueenSide = 2,
    BlackKingSide = 4, BlackQueenSide = 8, All = 15
};
[[nodiscard]] constexpr CastlingRights operator|(CastlingRights a, CastlingRights b) noexcept {
    return static_cast<CastlingRights>(static_cast<unsigned>(a) | static_cast<unsigned>(b));
}
[[nodiscard]] constexpr bool hasRight(CastlingRights rights, CastlingRights right) noexcept {
    const auto mask = static_cast<unsigned>(right);
    return mask != 0 && (static_cast<unsigned>(rights) & mask) == mask;
}

// Reversible metadata. No pointers, ownership, or history inside a position.
struct GameState {
    std::uint32_t halfmoveClock = 0;
    std::uint32_t fullmoveNumber = 1;
    Color sideToMove = Color::White;
    CastlingRights castlingRights = CastlingRights::None;
    Square enPassantSquare = Square::None;
    friend constexpr bool operator==(const GameState&, const GameState&) noexcept = default;
};

struct UndoState {
    GameState previous;
    Piece captured = Piece::None;
};

class Position {
public:
    inline static constexpr std::string_view StartFen =
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

    Position() noexcept; // Orthodox starting position, always valid.
    // Checked boundary: failure returns nullopt; optional diagnostics are owned by caller.
    [[nodiscard]] static std::optional<Position> fromFen(std::string_view fen, std::string* error = nullptr);
    [[nodiscard]] std::string toFen() const;
    // Precondition: move came from generatePseudoLegal for this position.
    // Does not reject self-check. Undo records must be consumed in reverse order.
    [[nodiscard]] UndoState makeMove(Move move) noexcept;
    void unmakeMove(Move move, const UndoState& undo) noexcept;

    [[nodiscard]] Piece pieceAt(Square square) const noexcept { return board_[index(square)]; }
    [[nodiscard]] Bitboard pieces(PieceType type) const noexcept {
        assert(isValid(type));
        return type == PieceType::None ? 0 : byType_[static_cast<unsigned>(type) - 1U];
    }
    [[nodiscard]] Bitboard pieces(Color color) const noexcept {
        assert(isValid(color));
        return byColor_[static_cast<unsigned>(color)];
    }
    [[nodiscard]] Bitboard pieces(Color color, PieceType type) const noexcept {
        return pieces(color) & pieces(type);
    }
    [[nodiscard]] Bitboard occupancy() const noexcept { return byColor_[0] | byColor_[1]; }
    [[nodiscard]] Square kingSquare(Color color) const noexcept { return leastSquare(pieces(color, PieceType::King)); }
    [[nodiscard]] Color sideToMove() const noexcept { return state_.sideToMove; }
    [[nodiscard]] CastlingRights castlingRights() const noexcept { return state_.castlingRights; }
    [[nodiscard]] Square enPassantSquare() const noexcept { return state_.enPassantSquare; }
    [[nodiscard]] std::uint32_t halfmoveClock() const noexcept { return state_.halfmoveClock; }
    [[nodiscard]] std::uint32_t fullmoveNumber() const noexcept { return state_.fullmoveNumber; }
    [[nodiscard]] GameState state() const noexcept { return state_; }
    [[nodiscard]] bool isConsistent() const noexcept { return validationError() == nullptr; }
    friend bool operator==(const Position&, const Position&) noexcept = default;

private:
    struct EmptyTag {};
    explicit Position(EmptyTag) noexcept {} // Only for transactional construction.
    void putPiece(Piece piece, Square square) noexcept;
    void removePiece(Square square) noexcept;
    [[nodiscard]] const char* validationError() const noexcept;

    std::array<Bitboard, 6> byType_{};  // Pawn through King; both colors combined.
    std::array<Bitboard, 2> byColor_{};
    std::array<Piece, 64> board_{};    // Direct square lookup, synchronized with bitboards.
    GameState state_{};
};
static_assert(std::is_trivially_copyable_v<GameState>);
static_assert(std::is_trivially_copyable_v<Position> && std::is_standard_layout_v<Position>);
} // namespace chess
