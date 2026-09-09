#include "board/bitboard.hpp"
#include "board/move.hpp"
#include "test_support.hpp"

using namespace chess;
static_assert(index(Square::H8) == 63);
static_assert(Move(Square::E2, Square::E4, MoveFlag::DoublePawnPush).isDoublePawnPush());
static_assert(count(FileA) == 8);

void types() {
    for (auto color : {Color::White, Color::Black}) {
        CHECK(opposite(opposite(color)) == color);
        CHECK(opposite(color) != color);
        for (unsigned t = 0; t <= 6; ++t) {
            const auto type = static_cast<PieceType>(t);
            const auto piece = makePiece(color, type);
            CHECK(typeOf(piece) == type);
            if (t == 0) CHECK(!colorOf(piece));
            else CHECK(colorOf(piece) == color);
        }
    }
    CHECK(!isValid(static_cast<Color>(2)));
    CHECK(!isValid(static_cast<Piece>(13)));
    CHECK(!isValid(static_cast<PieceType>(7)));
}
void squares() {
    for (int rank = 0; rank < 8; ++rank) for (int file = 0; file < 8; ++file) {
        const auto square = makeSquare(file, rank);
        CHECK(index(square) == static_cast<unsigned>(rank * 8 + file));
        CHECK(fileOf(square) == static_cast<unsigned>(file));
        CHECK(rankOf(square) == static_cast<unsigned>(rank));
        const auto name = squareName(square);
        CHECK(parseSquare(std::string_view(name.data(), name.size())) == square);
    }
    for (auto text : {"", "a", "a10", "A1", "i1", "a0", "a9", " a1"}) CHECK(parseSquare(text) == Square::None);
    CHECK(makeSquare(-1, 0) == Square::None);
    CHECK(makeSquare(0, -1) == Square::None);
    CHECK(makeSquare(8, 0) == Square::None);
    CHECK(makeSquare(0, 8) == Square::None);
    CHECK(!isValid(Square::None));
}
void moves() {
    CHECK(sizeof(Move) == 2);
    CHECK(Move{}.isNone());
    CHECK(Move{}.from() == Square::None && Move{}.to() == Square::None);
    CHECK(!Move{}.isCapture() && !Move{}.isPromotion());
    CHECK(Move{}.promotedPiece() == PieceType::None);
    constexpr std::array promotions{PieceType::Knight, PieceType::Bishop, PieceType::Rook, PieceType::Queen};
    // Every 16-bit input, including reserved flags and identical endpoints.
    for (unsigned raw = 0; raw < 65536; ++raw) {
        const auto decoded = Move::fromRaw(static_cast<std::uint16_t>(raw));
        const unsigned from = raw % 64, to = (raw / 64) % 64, flag = raw / 4096;
        const bool valid = raw == 0 || (from != to && flag != 6 && flag != 7);
        CHECK(decoded.has_value() == valid);
        if (!decoded || raw == 0) continue;
        const Move move(static_cast<Square>(from), static_cast<Square>(to), static_cast<MoveFlag>(flag));
        CHECK(move == *decoded);
        CHECK(move.raw() == raw);
        CHECK(index(move.from()) == from && index(move.to()) == to);
        CHECK(move.isCapture() == (flag == 4 || flag == 5 || flag >= 12));
        CHECK(move.isPromotion() == (flag >= 8));
        CHECK(move.isEnPassant() == (flag == 5));
        CHECK(move.isCastling() == (flag == 2 || flag == 3));
        CHECK(move.isDoublePawnPush() == (flag == 1));
        CHECK(move.promotedPiece() == (flag >= 8 ? promotions[flag % 4] : PieceType::None));
    }
    CHECK(Move(Square::E2, Square::E4, MoveFlag::DoublePawnPush).raw() == 0x170C);
}
void bitboards() {
    using Shift = Bitboard (*)(Bitboard) noexcept;
    constexpr std::array<Shift, 8> shifts{north, south, east, west, northEast, northWest, southEast, southWest};
    constexpr std::array<int, 8> dx{0, 0, 1, -1, 1, -1, 1, -1};
    constexpr std::array<int, 8> dy{1, -1, 0, 0, 1, 1, -1, -1};
    for (int r = 0; r < 8; ++r) for (int f = 0; f < 8; ++f) {
        const auto square = makeSquare(f, r);
        Bitboard board = 0;
        set(board, square);
        CHECK(count(board) == 1 && contains(board, square));
        CHECK(leastSquare(board) == square);
        CHECK((board & fileMask(static_cast<unsigned>(f))) != 0);
        CHECK((board & rankMask(static_cast<unsigned>(r))) != 0);
        for (unsigned d = 0; d < shifts.size(); ++d) {
            const auto target = makeSquare(f + dx[d], r + dy[d]);
            CHECK(shifts[d](board) == (isValid(target) ? bit(target) : 0));
        }
        CHECK(popLeastSquare(board) == square && board == 0);
        set(board, square); set(board, square);
        clear(board, square); clear(board, square);
        CHECK(board == 0);
    }
    Bitboard full = ~Bitboard{0};
    CHECK(count(full) == 64);
    for (unsigned i = 0; i < 64; ++i) CHECK(popLeastSquare(full) == static_cast<Square>(i));
    CHECK(popLeastSquare(full) == Square::None && full == 0);
    CHECK(leastSquare(0) == Square::None && count(0) == 0);
    CHECK(fileMask(7) == FileH && rankMask(7) == Rank8);
}
int main(int argc, char** argv) {
    return test::run(argc, argv, std::array{
        test::Case{"types", types}, test::Case{"squares", squares},
        test::Case{"moves", moves}, test::Case{"bitboards", bitboards}});
}
