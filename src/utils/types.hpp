#pragma once

#include <array>
#include <cassert>
#include <cstdint>
#include <optional>
#include <string_view>

namespace chess {

enum class Color : std::uint8_t { White, Black };
[[nodiscard]] constexpr bool isValid(Color color) noexcept {
    return color == Color::White || color == Color::Black;
}
[[nodiscard]] constexpr Color opposite(Color color) noexcept {
    assert(isValid(color));
    return color == Color::White ? Color::Black : Color::White;
}

enum class PieceType : std::uint8_t { None, Pawn, Knight, Bishop, Rook, Queen, King };
[[nodiscard]] constexpr bool isValid(PieceType type) noexcept {
    return static_cast<unsigned>(type) <= 6;
}

// Dense codes: empty=0, white=1..6, black=7..12.
enum class Piece : std::uint8_t {
    None, WhitePawn, WhiteKnight, WhiteBishop, WhiteRook, WhiteQueen, WhiteKing,
    BlackPawn, BlackKnight, BlackBishop, BlackRook, BlackQueen, BlackKing
};
[[nodiscard]] constexpr bool isValid(Piece piece) noexcept {
    return static_cast<unsigned>(piece) <= 12;
}
[[nodiscard]] constexpr Piece makePiece(Color color, PieceType type) noexcept {
    assert(isValid(color) && isValid(type));
    return type == PieceType::None ? Piece::None :
        static_cast<Piece>(static_cast<unsigned>(type) + (color == Color::Black ? 6U : 0U));
}
[[nodiscard]] constexpr PieceType typeOf(Piece piece) noexcept {
    assert(isValid(piece));
    const auto code = static_cast<unsigned>(piece);
    return code == 0 ? PieceType::None : static_cast<PieceType>((code - 1) % 6 + 1);
}
[[nodiscard]] constexpr std::optional<Color> colorOf(Piece piece) noexcept {
    assert(isValid(piece));
    if (piece == Piece::None) return std::nullopt;
    return static_cast<unsigned>(piece) <= 6 ? Color::White : Color::Black;
}

enum class Square : std::uint8_t {
    A1, B1, C1, D1, E1, F1, G1, H1,
    A2, B2, C2, D2, E2, F2, G2, H2,
    A3, B3, C3, D3, E3, F3, G3, H3,
    A4, B4, C4, D4, E4, F4, G4, H4,
    A5, B5, C5, D5, E5, F5, G5, H5,
    A6, B6, C6, D6, E6, F6, G6, H6,
    A7, B7, C7, D7, E7, F7, G7, H7,
    A8, B8, C8, D8, E8, F8, G8, H8,
    None
};
[[nodiscard]] constexpr bool isValid(Square square) noexcept {
    return static_cast<unsigned>(square) < 64;
}
[[nodiscard]] constexpr unsigned index(Square square) noexcept {
    assert(isValid(square));
    return static_cast<unsigned>(square);
}
[[nodiscard]] constexpr unsigned fileOf(Square square) noexcept { return index(square) & 7U; }
[[nodiscard]] constexpr unsigned rankOf(Square square) noexcept { return index(square) >> 3U; }
// Checked boundary factory; invalid coordinates never wrap onto the board.
[[nodiscard]] constexpr Square makeSquare(int file, int rank) noexcept {
    if (file < 0 || file > 7 || rank < 0 || rank > 7) return Square::None;
    return static_cast<Square>(rank * 8 + file);
}
[[nodiscard]] constexpr Square parseSquare(std::string_view text) noexcept {
    if (text.size() != 2 || text[0] < 'a' || text[0] > 'h' || text[1] < '1' || text[1] > '8')
        return Square::None;
    return makeSquare(text[0] - 'a', text[1] - '1');
}
[[nodiscard]] constexpr std::array<char, 2> squareName(Square square) noexcept {
    assert(isValid(square));
    return {static_cast<char>('a' + fileOf(square)), static_cast<char>('1' + rankOf(square))};
}

static_assert(sizeof(Color) == 1 && sizeof(PieceType) == 1 && sizeof(Piece) == 1);
static_assert(sizeof(Square) == 1);
} // namespace chess
