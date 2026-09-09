#include "movegen/perft.hpp"
#include "test_support.hpp"
using namespace chess;
namespace {
Position parse(std::string_view fen) { const auto p = Position::fromFen(fen); CHECK(p); return *p; }
void perftTests() {
    const attacks::SlidingAttacks sliding;
    struct Case { std::string_view fen; int depth; std::uint64_t expected; };
    for (const auto& test : std::array{
        Case{Position::StartFen, 0, 1}, Case{Position::StartFen, 1, 20},
        Case{Position::StartFen, 2, 400}, Case{Position::StartFen, 3, 8902}, Case{Position::StartFen, 4, 197281},
        Case{"r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 3, 97862},
        Case{"8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 4, 43238},
        Case{"r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", 3, 9467},
        Case{"rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", 3, 62379}}) {
        auto p = parse(test.fen); const auto before = p;
        const auto actual = perft(p, test.depth, sliding);
        if (actual != test.expected) throw std::runtime_error(std::string(test.fen) + " actual=" + std::to_string(actual));
        CHECK(p == before);
    }
}
void undoTests() {
    const attacks::SlidingAttacks sliding;
    for (auto fen : {Position::StartFen,
        std::string_view("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 17 5"),
        std::string_view("r3k2r/8/8/8/8/8/8/R3K2R b KQkq - 17 5"),
        std::string_view("4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 2"),
        std::string_view("4k3/8/8/8/3Pp3/8/8/4K3 b - d3 0 2"),
        std::string_view("1r2k3/P7/8/8/8/8/8/4K3 w - - 0 1"),
        std::string_view("4k3/8/8/8/8/8/p7/1R2K3 b - - 0 1")}) {
        auto p = parse(fen); const auto before = p;
        MoveList moves; generatePseudoLegal(p, sliding, moves);
        for (const auto move : moves) {
            CHECK(isValid(move.from()) && isValid(move.to()));
            const auto undo = p.makeMove(move);
            CHECK(p.isConsistent());
            CHECK(p.sideToMove() == opposite(before.sideToMove()));
            CHECK(p.pieceAt(move.from()) == Piece::None);
            CHECK(p.pieceAt(move.to()) == (move.isPromotion() ? makePiece(before.sideToMove(), move.promotedPiece()) : before.pieceAt(move.from())));
            if (move.isEnPassant()) CHECK(p.pieceAt(makeSquare(static_cast<int>(fileOf(move.to())), static_cast<int>(rankOf(move.from())))) == Piece::None);
            p.unmakeMove(move, undo); CHECK(p == before); CHECK(p.toFen() == fen);
        }
    }
    Position p;
    const Move e4(Square::E2, Square::E4, MoveFlag::DoublePawnPush);
    const auto undo = p.makeMove(e4);
    CHECK(p.toFen() == "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1");
    p.unmakeMove(e4, undo); CHECK(p == Position{});
}
void legality() {
    const attacks::SlidingAttacks sliding;
    auto p = parse("k7/8/8/r4pPK/8/8/8/8 w - f6 0 1");
    MoveList moves; generateLegal(p, sliding, moves);
    for (const auto move : moves) CHECK(!move.isEnPassant());
    for (auto fen : {"7k/6Q1/5K2/8/8/8/8/8 b - - 0 1", "7k/5K2/6Q1/8/8/8/8/8 b - - 0 1"}) {
        p = parse(fen); generateLegal(p, sliding, moves); CHECK(moves.empty());
    }
}
}
int main(int argc, char** argv) {
    return test::run(argc, argv, std::array{test::Case{"perft", perftTests}, test::Case{"undo", undoTests}, test::Case{"legality", legality}});
}
