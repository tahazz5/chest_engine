#include "board/position.hpp"
#include "test_support.hpp"
#include <limits>

using namespace chess;
namespace {
constexpr std::array<std::string_view, 7> Positions{
    Position::StartFen,
    "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
    "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
    "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1",
    "rnbqkbnr/ppp1pppp/8/3p4/8/8/PPPPPPPP/RNBQKBNR w KQkq d6 0 2",
    "4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 2",
    "4k3/8/8/8/8/8/8/4K3 b - - 100 72"
};
Position parse(std::string_view fen) {
    std::string error;
    const auto result = Position::fromFen(fen, &error);
    if (!result) throw std::runtime_error(std::string(fen) + ": " + error);
    CHECK(error.empty());
    return *result;
}
void initial() {
    const Position p;
    CHECK(p.toFen() == Position::StartFen);
    CHECK(p == parse(Position::StartFen));
    CHECK(p.isConsistent());
    CHECK(p.sideToMove() == Color::White);
    CHECK(p.castlingRights() == CastlingRights::All);
    CHECK(p.enPassantSquare() == Square::None);
    CHECK(p.halfmoveClock() == 0 && p.fullmoveNumber() == 1);
    CHECK(p.pieces(Color::White) == 0xFFFFULL);
    CHECK(p.pieces(Color::Black) == 0xFFFF000000000000ULL);
    CHECK(p.occupancy() == 0xFFFF00000000FFFFULL);
    CHECK(p.pieces(PieceType::Pawn) == 0x00FF00000000FF00ULL);
    CHECK(p.pieces(Color::White, PieceType::Knight) == (bit(Square::B1) | bit(Square::G1)));
    CHECK(p.pieces(PieceType::None) == 0);
    CHECK(p.kingSquare(Color::White) == Square::E1);
    CHECK(p.kingSquare(Color::Black) == Square::E8);
    CHECK(p.pieceAt(Square::A1) == Piece::WhiteRook);
    CHECK(p.pieceAt(Square::D8) == Piece::BlackQueen);
    CHECK(p.pieceAt(Square::E4) == Piece::None);
    CHECK(!hasRight(CastlingRights::None, CastlingRights::None));
}
void roundtrip() {
    for (const auto fen : Positions) {
        const auto p = parse(fen);
        CHECK(p.toFen() == fen);
        CHECK(parse(p.toFen()) == p);
    }
    CHECK(parse(" \t4k3/8/8/8/8/8/8/4K3\n b  - - 0007 0009 \r").toFen() ==
          "4k3/8/8/8/8/8/8/4K3 b - - 7 9");
    // All 16 rights combinations, input order normalized to KQkq.
    for (unsigned mask = 0; mask < 16; ++mask) {
        std::string flags;
        for (int i = 3; i >= 0; --i) if ((mask & (1U << i)) != 0) flags += std::string_view("KQkq")[static_cast<unsigned>(i)];
        if (flags.empty()) flags = "-";
        const auto p = parse("r3k2r/8/8/8/8/8/8/R3K2R w " + flags + " - 0 1");
        CHECK(static_cast<unsigned>(p.castlingRights()) == mask);
        CHECK(parse(p.toFen()) == p);
    }
    const auto max = parse("4k3/8/8/8/8/8/8/4K3 w - - 4294967295 4294967295");
    CHECK(max.halfmoveClock() == std::numeric_limits<std::uint32_t>::max());
    CHECK(max.fullmoveNumber() == std::numeric_limits<std::uint32_t>::max());
    CHECK(parse(max.toFen()) == max);
}
void invalid() {
    constexpr std::array bad{
        "", "8/8/8/8/8/8/8/8 w - - 0 1", // no kings
        "4k3/8/8/8/8/8/8/4K3 w - - 0", // missing field
        "4k3/8/8/8/8/8/8/4K3 w - - 0 1 extra",
        "4k3/8/8/8/8/8/4K3 w - - 0 1", // seven ranks
        "4k3/8/8/8/8/8/8/8/4K3 w - - 0 1",
        "4k3/8/8/8/8/8/8/4K2 w - - 0 1",
        "4k3/8/8/8/8/8/8/4K4 w - - 0 1",
        "4k3/8/8/8/8/8/8/4K3/ w - - 0 1",
        "4k3/8/8/8/8/8/8/04K3 w - - 0 1",
        "4k3/8/8/8/8/8/8/4K111 w - - 0 1",
        "4k3/8/8/8/8/8/8/4X3 w - - 0 1",
        "4k3/8/8/8/8/8/8/4K3 W - - 0 1",
        "4k3/8/8/8/8/8/8/4K3 w KK - 0 1",
        "4k3/8/8/8/8/8/8/4K3 w A - 0 1",
        "4k3/8/8/8/8/8/8/4K3 w -K - 0 1",
        "4k3/8/8/8/8/8/8/4K3 w K - 0 1", // no rook
        "4k3/8/8/8/8/8/8/R2K3R w KQ - 0 1", // displaced king
        "4k3/8/8/8/8/8/8/4K2r w K - 0 1", // wrong rook color
        "4k3/8/8/8/8/8/8/4KK2 w - - 0 1",
        "4k3/8/8/8/8/8/8/P3K3 w - - 0 1",
        "p3k3/8/8/8/8/8/8/4K3 w - - 0 1",
        "4k3/8/8/8/P7/8/PPPPPPPP/4K3 w - - 0 1", // nine pawns
        "4k3/8/8/8/NNNNNNNN/NNNNNNNN/8/4K3 w - - 0 1", // 17 pieces
        "4k3/8/8/8/8/8/8/4K3 w - z9 0 1",
        "4k3/8/8/8/8/8/8/4K3 w - e3 0 1", // wrong rank
        "4k3/8/8/8/8/8/8/4K3 w - d6 0 1", // no pushed pawn
        "4k3/8/3N4/3p4/8/8/8/4K3 w - d6 0 1", // occupied target
        "4k3/3n4/8/3p4/8/8/8/4K3 w - d6 0 1", // occupied origin
        "4k3/8/8/3P4/8/8/8/4K3 w - d6 0 1", // wrong pawn color
        "4k3/8/8/3p4/8/8/8/4K3 w - d6 1 1", // nonzero clock
        "4k3/8/8/8/8/8/8/4K3 w - - -1 1",
        "4k3/8/8/8/8/8/8/4K3 w - - +1 1",
        "4k3/8/8/8/8/8/8/4K3 w - - 1x 1",
        "4k3/8/8/8/8/8/8/4K3 w - - 4294967296 1",
        "4k3/8/8/8/8/8/8/4K3 w - - 0 0",
        "4k3/8/8/8/8/8/8/4K3 w - - 0 -1",
        "4k3/8/8/8/8/8/8/4K3 w - - 0 4294967296"
    };
    for (const auto fen : bad) {
        std::string error;
        const auto p = Position::fromFen(fen, &error);
        if (p) throw std::runtime_error(std::string("Unexpectedly accepted: ") + fen);
        CHECK(!error.empty());
        CHECK(!Position::fromFen(fen));
    }
}
void invariants() {
    for (const auto fen : Positions) {
        const auto p = parse(fen);
        CHECK(p.isConsistent());
        CHECK((p.pieces(Color::White) & p.pieces(Color::Black)) == 0);
        Bitboard unionOfTypes = 0;
        for (unsigned t = 1; t <= 6; ++t) {
            const auto type = static_cast<PieceType>(t);
            CHECK((unionOfTypes & p.pieces(type)) == 0);
            unionOfTypes |= p.pieces(type);
        }
        CHECK(unionOfTypes == p.occupancy());
        for (unsigned s = 0; s < 64; ++s) {
            const auto square = static_cast<Square>(s);
            const auto piece = p.pieceAt(square);
            CHECK(contains(p.occupancy(), square) == (piece != Piece::None));
            if (piece != Piece::None) CHECK(contains(p.pieces(*colorOf(piece), typeOf(piece)), square));
        }
        auto stateCopy = p.state();
        stateCopy.sideToMove = opposite(stateCopy.sideToMove);
        CHECK(stateCopy != p.state());
    }
}
void lifetime() {
    Position p;
    {
        std::string input(Positions[2]);
        p = parse(input);
        input.assign(1000, 'x'); // Position keeps no views into input.
    }
    CHECK(p.toFen() == Positions[2]);
    const auto copy = p;
    p = Position{};
    CHECK(copy.toFen() == Positions[2]);
    CHECK(p.toFen() == Position::StartFen);
    std::string aliased(Positions[2]);
    const auto parsed = Position::fromFen(aliased, &aliased);
    CHECK(parsed && aliased.empty());
    CHECK(parsed->toFen() == Positions[2]);
    aliased = "bad FEN";
    CHECK(!Position::fromFen(aliased, &aliased));
    CHECK(!aliased.empty());
}
}
int main(int argc, char** argv) {
    return test::run(argc, argv, std::array{
        test::Case{"initial", initial}, test::Case{"roundtrip", roundtrip},
        test::Case{"invalid", invalid}, test::Case{"invariants", invariants}, test::Case{"lifetime", lifetime}});
}
