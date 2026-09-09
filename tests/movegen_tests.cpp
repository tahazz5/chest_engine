#include "movegen/movegen.hpp"
#include "test_support.hpp"
#include <algorithm>
#include <cstdlib>
#include <vector>

using namespace chess;
namespace {
Position parse(std::string_view fen) {
    std::string error;
    const auto p = Position::fromFen(fen, &error);
    if (!p) throw std::runtime_error(error);
    return *p;
}
MoveList generate(const Position& p) {
    const attacks::SlidingAttacks sliding;
    MoveList moves;
    generatePseudoLegal(p, sliding, moves);
    return moves;
}
bool has(const MoveList& moves, Square from, Square to, MoveFlag flag = MoveFlag::Quiet) {
    return std::find(moves.begin(), moves.end(), Move(from, to, flag)) != moves.end();
}
std::vector<std::uint16_t> sorted(const MoveList& moves) {
    std::vector<std::uint16_t> result;
    for (auto move : moves) result.push_back(move.raw());
    std::sort(result.begin(), result.end());
    CHECK(std::adjacent_find(result.begin(), result.end()) == result.end());
    return result;
}
void list() {
    MoveList moves;
    CHECK(moves.empty() && moves.view().empty() && moves.begin() == moves.end());
    CHECK(!moves.tryPush(Move{}));
    const Move m(Square::E2, Square::E4, MoveFlag::DoublePawnPush);
    for (std::size_t i = 0; i < MoveList::Capacity; ++i) CHECK(moves.tryPush(m));
    CHECK(moves.size() == MoveList::Capacity);
    CHECK(!moves.tryPush(m));
    CHECK(moves[0] == m && moves[moves.size() - 1] == m);
    auto copy = moves;
    moves.clear();
    CHECK(moves.empty() && copy.size() == MoveList::Capacity);
    const attacks::SlidingAttacks sliding;
    generatePseudoLegal(Position{}, sliding, copy);
    CHECK(copy.size() == 20); // replacement, not append
    std::cout << "sizeof/alignof MoveList: " << sizeof(MoveList) << '/' << alignof(MoveList) << '\n';
}
void initial() {
    for (auto side : {"w", "b"}) {
        const auto p = parse(std::string("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR ") + side + " KQkq - 0 1");
        const auto moves = generate(p);
        CHECK(moves.size() == 20);
        const int rank = p.sideToMove() == Color::White ? 1 : 6;
        const int forward = rank == 1 ? 1 : -1;
        for (int file = 0; file < 8; ++file) {
            CHECK(has(moves, makeSquare(file, rank), makeSquare(file, rank + forward)));
            CHECK(has(moves, makeSquare(file, rank), makeSquare(file, rank + 2 * forward), MoveFlag::DoublePawnPush));
        }
        unsigned knightMoves = 0;
        for (auto move : moves) {
            CHECK(!move.isCapture() && !move.isPromotion() && !move.isCastling());
            if (typeOf(p.pieceAt(move.from())) == PieceType::Knight) ++knightMoves;
        }
        CHECK(knightMoves == 4);
        (void)sorted(moves);
    }
}
void pawns() {
    const auto blocked = generate(parse("4k3/8/8/8/1n6/P7/PP6/4K3 w - - 0 1"));
    CHECK(!has(blocked, Square::A2, Square::A3));
    CHECK(!has(blocked, Square::A2, Square::A4, MoveFlag::DoublePawnPush));
    CHECK(has(blocked, Square::B2, Square::B3));
    CHECK(!has(blocked, Square::B2, Square::B4, MoveFlag::DoublePawnPush));
    CHECK(has(blocked, Square::A3, Square::B4, MoveFlag::Capture));
    const auto white = generate(parse("1r2k3/P7/8/8/8/8/8/4K3 w - - 0 1"));
    const auto black = generate(parse("4k3/8/8/8/8/8/p7/1R2K3 b - - 0 1"));
    for (unsigned i = 0; i < 4; ++i) {
        CHECK(has(white, Square::A7, Square::A8, static_cast<MoveFlag>(8 + i)));
        CHECK(has(white, Square::A7, Square::B8, static_cast<MoveFlag>(12 + i)));
        CHECK(has(black, Square::A2, Square::A1, static_cast<MoveFlag>(8 + i)));
        CHECK(has(black, Square::A2, Square::B1, static_cast<MoveFlag>(12 + i)));
    }
    CHECK(!has(white, Square::A7, Square::A8));
    const auto ep = generate(parse("4k3/8/8/2PpP3/8/8/8/4K3 w - d6 0 2"));
    CHECK(has(ep, Square::C5, Square::D6, MoveFlag::EnPassant));
    CHECK(has(ep, Square::E5, Square::D6, MoveFlag::EnPassant));
    const auto blackEp = generate(parse("4k3/8/8/8/2pPp3/8/8/4K3 b - d3 0 2"));
    CHECK(has(blackEp, Square::C4, Square::D3, MoveFlag::EnPassant));
    CHECK(has(blackEp, Square::E4, Square::D3, MoveFlag::EnPassant));
    const auto noEp = generate(parse("4k3/8/8/2PpP3/8/8/8/4K3 w - - 0 2"));
    for (auto move : noEp) CHECK(!move.isEnPassant());
}
void castling() {
    const auto white = generate(parse("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1"));
    CHECK(has(white, Square::E1, Square::G1, MoveFlag::KingCastle));
    CHECK(has(white, Square::E1, Square::C1, MoveFlag::QueenCastle));
    const auto black = generate(parse("r3k2r/8/8/8/8/8/8/R3K2R b KQkq - 0 1"));
    CHECK(has(black, Square::E8, Square::G8, MoveFlag::KingCastle));
    CHECK(has(black, Square::E8, Square::C8, MoveFlag::QueenCastle));
    for (auto fen : {
        "4k3/8/8/8/8/8/8/R3K2R w - - 0 1", // no rights
        "k3r3/8/8/8/8/8/8/R3K2R w KQ - 0 1", // in check
        "4k3/8/8/8/8/8/8/RN2KB1R w KQ - 0 1" // occupied b1/f1
    }) for (auto move : generate(parse(fen))) CHECK(!move.isCastling());
    for (auto fen : {"k4r2/8/8/8/8/8/8/R3K2R w KQ - 0 1", // f1 attacked
                     "k5r1/8/8/8/8/8/8/R3K2R w KQ - 0 1"}) { // g1 attacked
        const auto moves = generate(parse(fen));
        CHECK(!has(moves, Square::E1, Square::G1, MoveFlag::KingCastle));
        CHECK(has(moves, Square::E1, Square::C1, MoveFlag::QueenCastle));
    }
    for (auto fen : {"3rk3/8/8/8/8/8/8/R3K2R w KQ - 0 1",
                     "2r1k3/8/8/8/8/8/8/R3K2R w KQ - 0 1"}) {
        const auto moves = generate(parse(fen));
        CHECK(!has(moves, Square::E1, Square::C1, MoveFlag::QueenCastle));
        CHECK(has(moves, Square::E1, Square::G1, MoveFlag::KingCastle));
    }
    // b1 and the rook may be attacked: only the king's path must be safe.
    CHECK(has(generate(parse("1r2k3/8/8/8/8/8/8/R3K3 w Q - 0 1")), Square::E1, Square::C1, MoveFlag::QueenCastle));
    CHECK(has(generate(parse("4k2r/8/8/8/8/8/8/4K2R w K - 0 1")), Square::E1, Square::G1, MoveFlag::KingCastle));
}
// Mailbox oracle for ordinary moves, independent of attack tables and bit iteration.
std::vector<std::uint16_t> oracle(const Position& p) {
    std::vector<std::uint16_t> result;
    const auto us = p.sideToMove();
    const int forward = us == Color::White ? 1 : -1;
    for (int f = 0; f < 64; ++f) for (int t = 0; t < 64; ++t) {
        if (f == t) continue;
        const auto from = static_cast<Square>(f), to = static_cast<Square>(t);
        const auto piece = p.pieceAt(from), target = p.pieceAt(to);
        if (colorOf(piece) != us || colorOf(target) == us || typeOf(target) == PieceType::King) continue;
        const int dx = t % 8 - f % 8, dy = t / 8 - f / 8;
        const auto type = typeOf(piece);
        bool valid = false;
        MoveFlag flag = target == Piece::None ? MoveFlag::Quiet : MoveFlag::Capture;
        if (type == PieceType::Pawn) {
            if (dx == 0 && target == Piece::None) {
                valid = dy == forward;
                if (dy == 2 * forward && f / 8 == (us == Color::White ? 1 : 6) &&
                    p.pieceAt(makeSquare(f % 8, f / 8 + forward)) == Piece::None) {
                    valid = true; flag = MoveFlag::DoublePawnPush;
                }
            } else if (std::abs(dx) == 1 && dy == forward) {
                valid = target != Piece::None;
                if (to == p.enPassantSquare()) { valid = true; flag = MoveFlag::EnPassant; }
            }
        } else if (type == PieceType::Knight) valid = std::abs(dx) * std::abs(dy) == 2;
        else if (type == PieceType::King) valid = std::max(std::abs(dx), std::abs(dy)) == 1;
        else {
            const bool straight = dx == 0 || dy == 0, diagonal = std::abs(dx) == std::abs(dy);
            valid = (type == PieceType::Rook && straight) || (type == PieceType::Bishop && diagonal) ||
                    (type == PieceType::Queen && (straight || diagonal));
            if (valid) {
                const int distance = std::max(std::abs(dx), std::abs(dy));
                for (int i = 1; i < distance; ++i)
                    if (p.pieceAt(makeSquare(f % 8 + dx / distance * i, f / 8 + dy / distance * i)) != Piece::None) valid = false;
            }
        }
        if (!valid) continue;
        if (type == PieceType::Pawn && t / 8 == (us == Color::White ? 7 : 0)) {
            for (unsigned i = 0; i < 4; ++i)
                result.push_back(Move(from, to, static_cast<MoveFlag>((target == Piece::None ? 8U : 12U) + i)).raw());
        } else result.push_back(Move(from, to, flag).raw());
    }
    std::sort(result.begin(), result.end());
    return result;
}
void oracleTests() {
    const attacks::SlidingAttacks portable(attacks::SlidingBackend::PortableTable);
    const attacks::SlidingAttacks fast(attacks::SlidingBackend::Auto);
    for (auto fen : {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
        "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
        "4k3/8/8/2PpP3/8/8/8/4K3 w - d6 0 2",
        "4k3/8/8/8/2pPp3/8/8/4K3 b - d3 0 2",
        "1r2k3/P7/8/8/8/8/8/4K3 w - - 0 1",
        "4k3/8/8/8/8/8/p7/1R2K3 b - - 0 1",
        "k5r1/7P/8/8/8/8/8/K7 w - - 0 1",
        "k7/8/8/8/QQQQQQQQ/QQQQQQQ1/8/7K w - - 0 1",
        "k3r3/8/8/8/8/8/4R3/4K3 w - - 0 1",
        "k7/8/8/r4pPK/8/8/8/8 w - f6 0 1",
        "4k3/8/8/8/8/8/8/3QK3 w - - 0 1"
    }) {
        const auto p = parse(fen), saved = p;
        const auto reference = generate(p);
        MoveList ordinary;
        for (auto move : reference) {
            if (!move.isCastling()) ordinary.push(move);
            CHECK(colorOf(p.pieceAt(move.from())) == p.sideToMove());
            CHECK(colorOf(p.pieceAt(move.to())) != p.sideToMove());
            CHECK(typeOf(p.pieceAt(move.to())) != PieceType::King);
            CHECK(move.isCapture() == (p.pieceAt(move.to()) != Piece::None || move.isEnPassant()));
        }
        CHECK(sorted(ordinary) == oracle(p));
        MoveList candidate;
        generatePseudoLegal(p, portable, candidate);
        CHECK(sorted(candidate) == sorted(reference));
        generatePseudoLegal(p, fast, candidate);
        CHECK(sorted(candidate) == sorted(reference));
        CHECK(p == saved);
    }
}
void pseudoBoundary() {
    const auto pinned = generate(parse("k3r3/8/8/8/8/8/4R3/4K3 w - - 0 1"));
    CHECK(has(pinned, Square::E2, Square::D2)); // exposes own king, filtered in phase 5
    const auto ep = generate(parse("k7/8/8/r4pPK/8/8/8/8 w - f6 0 1"));
    CHECK(has(ep, Square::G5, Square::F6, MoveFlag::EnPassant)); // opens rook line
    const auto king = generate(parse("k4r2/8/8/8/8/8/8/4K3 w - - 0 1"));
    CHECK(has(king, Square::E1, Square::F1)); // attacked destination
    const auto enemyKing = generate(parse("4k3/8/8/8/8/8/8/3QK3 w - - 0 1"));
    for (auto move : enemyKing) CHECK(move.to() != Square::E8);
}
}
int main(int argc, char** argv) {
    return test::run(argc, argv, std::array{
        test::Case{"list", list}, test::Case{"initial", initial}, test::Case{"pawns", pawns},
        test::Case{"castling", castling}, test::Case{"oracle", oracleTests}, test::Case{"boundary", pseudoBoundary}});
}
