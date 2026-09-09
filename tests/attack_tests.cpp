#include "board/attacks.hpp"
#include "board/position.hpp"
#include "test_support.hpp"
#include <algorithm>
#include <cstdlib>

using namespace chess;
using namespace chess::attacks;
namespace {
// Independent oracle: examine every target and the squares strictly between endpoints.
Bitboard oracleSlider(Square origin, Bitboard occupancy, bool diagonal) {
    Bitboard result = 0;
    const int of = static_cast<int>(fileOf(origin)), orank = static_cast<int>(rankOf(origin));
    for (int t = 0; t < 64; ++t) {
        const int dx = t % 8 - of, dy = t / 8 - orank;
        if (dx == 0 && dy == 0) continue;
        const bool aligned = diagonal ? std::abs(dx) == std::abs(dy) : dx == 0 || dy == 0;
        if (!aligned) continue;
        const int steps = std::max(std::abs(dx), std::abs(dy));
        bool blocked = false;
        for (int i = 1; i < steps; ++i) {
            const auto between = makeSquare(of + dx / steps * i, orank + dy / steps * i);
            if (contains(occupancy, between)) { blocked = true; break; }
        }
        if (!blocked) result |= bit(static_cast<Square>(t));
    }
    return result;
}
Bitboard oracleLeaper(Square origin, PieceType type, Color color = Color::White) {
    Bitboard result = 0;
    for (int t = 0; t < 64; ++t) {
        const int dx = t % 8 - static_cast<int>(fileOf(origin));
        const int dy = t / 8 - static_cast<int>(rankOf(origin));
        bool attacked = false;
        if (type == PieceType::Knight) attacked = std::abs(dx) * std::abs(dy) == 2;
        if (type == PieceType::King) attacked = std::max(std::abs(dx), std::abs(dy)) == 1;
        if (type == PieceType::Pawn) attacked = std::abs(dx) == 1 && dy == (color == Color::White ? 1 : -1);
        if (attacked) result |= bit(static_cast<Square>(t));
    }
    return result;
}
void leapers() {
    static_assert(knight(Square::A1) == (bit(Square::B3) | bit(Square::C2)));
    static_assert(king(Square::H8) == (bit(Square::G8) | bit(Square::G7) | bit(Square::H7)));
    static_assert(pawn(Color::White, Square::H8) == 0);
    for (unsigned s = 0; s < 64; ++s) {
        const auto square = static_cast<Square>(s);
        CHECK(knight(square) == oracleLeaper(square, PieceType::Knight));
        CHECK(king(square) == oracleLeaper(square, PieceType::King));
        for (const auto color : {Color::White, Color::Black})
            CHECK(pawn(color, square) == oracleLeaper(square, PieceType::Pawn, color));
    }
    CHECK(count(knight(Square::D4)) == 8);
    CHECK(count(king(Square::A1)) == 3);
    CHECK(pawn(Color::White, Square::E2) == (bit(Square::D3) | bit(Square::F3)));
    CHECK(pawn(Color::Black, Square::E7) == (bit(Square::D6) | bit(Square::F6)));
}
void rays() {
    CHECK(rookRays(Square::A1, 0) == ((FileA | Rank1) & ~bit(Square::A1)));
    CHECK(bishopRays(Square::A1, 0) == 0x8040201008040200ULL);
    const Bitboard blocked = bit(Square::D6) | bit(Square::F4) | bit(Square::D2) | bit(Square::B4);
    const Bitboard expected = bit(Square::D5) | bit(Square::D6) | bit(Square::E4) | bit(Square::F4) |
                              bit(Square::D3) | bit(Square::D2) | bit(Square::C4) | bit(Square::B4);
    CHECK(rookRays(Square::D4, blocked) == expected);
    for (unsigned s = 0; s < 64; ++s) {
        const auto square = static_cast<Square>(s);
        for (const auto occupied : {Bitboard{0}, ~Bitboard{0}, bit(square), Bitboard{0xAA55AA55AA55AA55ULL}}) {
            CHECK(rookRays(square, occupied) == oracleSlider(square, occupied, false));
            CHECK(bishopRays(square, occupied) == oracleSlider(square, occupied, true));
            CHECK(queenRays(square, occupied) == (rookRays(square, occupied) | bishopRays(square, occupied)));
        }
    }
}
std::uint64_t randomBits(std::uint64_t& state) {
    state ^= state << 13U; state ^= state >> 7U; state ^= state << 17U;
    return state;
}
void tables() {
    static_assert(extractBits(0b101000, 0b111000) == 0b101);
    CHECK(extractBits(~Bitboard{0}, ~Bitboard{0}) == ~Bitboard{0});
    CHECK(extractBits(~Bitboard{0}, 0) == 0);
    const SlidingAttacks raysBackend;
    const SlidingAttacks portable(SlidingBackend::PortableTable);
    const SlidingAttacks pext(SlidingBackend::PextTable);
    const SlidingAttacks automatic(SlidingBackend::Auto);
    CHECK(raysBackend.backend() == SlidingBackend::Rays && raysBackend.tableBytes() == 0);
    CHECK(portable.backend() == SlidingBackend::PortableTable && portable.tableBytes() >= 861184);
    CHECK(pext.backend() == (hardwarePextAvailable() ? SlidingBackend::PextTable : SlidingBackend::PortableTable));
    CHECK(automatic.backend() == (hardwarePextAvailable() ? SlidingBackend::PextTable : SlidingBackend::Rays));
    std::array<std::size_t, 2> totals{};
    for (const bool diagonal : {false, true}) {
        for (unsigned s = 0; s < 64; ++s) {
            const auto square = static_cast<Square>(s);
            const auto mask = diagonal ? bishopRelevantMask(square) : rookRelevantMask(square);
            // Independent relevant mask: retain targets with another square beyond them.
            Bitboard expectedMask = 0;
            auto targets = oracleSlider(square, 0, diagonal);
            while (targets != 0) {
                const auto target = popLeastSquare(targets);
                const int dx = static_cast<int>(fileOf(target)) - static_cast<int>(fileOf(square));
                const int dy = static_cast<int>(rankOf(target)) - static_cast<int>(rankOf(square));
                const auto beyond = makeSquare(static_cast<int>(fileOf(target)) + (dx > 0) - (dx < 0),
                                               static_cast<int>(rankOf(target)) + (dy > 0) - (dy < 0));
                if (isValid(beyond)) expectedMask |= bit(target);
            }
            CHECK(mask == expectedMask);
            // Deposit integer bits by scanning all 64 squares, independently of subset enumeration in production.
            const unsigned combinations = 1U << count(mask);
            totals[diagonal ? 1U : 0U] += combinations;
            for (unsigned i = 0; i < combinations; ++i) {
                Bitboard occupied = 0;
                unsigned inputBit = 0;
                for (unsigned t = 0; t < 64; ++t) {
                    if ((mask & (Bitboard{1} << t)) == 0) continue;
                    if ((i & (1U << inputBit)) != 0) occupied |= Bitboard{1} << t;
                    ++inputBit;
                }
                CHECK(extractBits(occupied, mask) == i);
                const auto expected = oracleSlider(square, occupied, diagonal);
                for (const auto* backend : {&raysBackend, &portable, &pext}) {
                    CHECK((diagonal ? backend->bishop(square, occupied) : backend->rook(square, occupied)) == expected);
                    // All irrelevant bits set: origin, terminal edges, and off-ray squares.
                    CHECK((diagonal ? backend->bishop(square, occupied | ~mask) : backend->rook(square, occupied | ~mask)) == expected);
                }
            }
        }
    }
    CHECK(totals[0] == SlidingAttacks::RookEntries && totals[1] == SlidingAttacks::BishopEntries);
    std::uint64_t rng = 0x853C49E6748FEA9BULL;
    for (unsigned s = 0; s < 64; ++s) for (unsigned i = 0; i < 128; ++i) {
        const auto square = static_cast<Square>(s);
        const auto occupancy = randomBits(rng);
        const auto expected = oracleSlider(square, occupancy, false) | oracleSlider(square, occupancy, true);
        CHECK(portable.queen(square, occupancy) == expected);
        CHECK(pext.queen(square, occupancy) == expected);
        CHECK(automatic.queen(square, occupancy) == expected);
    }
    std::cout << "hardware PEXT: " << hardwarePextAvailable() << ", table bytes: " << portable.tableBytes() << '\n';
}
Position parse(std::string_view fen) {
    const auto p = Position::fromFen(fen);
    CHECK(p.has_value());
    return *p;
}
void queries() {
    const SlidingAttacks sliding;
    CHECK(!inCheck(Position{}, Color::White, sliding));
    CHECK(!inCheck(Position{}, Color::Black, sliding));
    const auto rookCheck = parse("k3r3/8/8/8/8/8/8/4K3 w - - 0 1");
    CHECK(inCheck(rookCheck, Color::White, sliding));
    CHECK(attackersTo(rookCheck, Square::E1, Color::Black, sliding) == bit(Square::E8));
    const auto blocked = parse("k3r3/8/8/8/8/4P3/8/4K3 w - - 0 1");
    CHECK(!inCheck(blocked, Color::White, sliding));
    const auto defended = parse("4k3/8/8/8/8/8/P7/R3K3 w - - 0 1");
    CHECK(attackersTo(defended, Square::A2, Color::White, sliding) == bit(Square::A1));
    const auto whitePawn = parse("4k3/8/8/8/8/4P3/8/K7 w - - 0 1");
    CHECK(attackersTo(whitePawn, Square::D4, Color::White, sliding) == bit(Square::E3));
    CHECK(!isSquareAttacked(whitePawn, Square::D2, Color::White, sliding));
    const auto blackPawn = parse("k7/8/4p3/8/8/8/8/4K3 w - - 0 1");
    CHECK(attackersTo(blackPawn, Square::D5, Color::Black, sliding) == bit(Square::E6));
    CHECK(!isSquareAttacked(blackPawn, Square::D7, Color::Black, sliding));
    const auto pinned = parse("4k3/4n3/8/8/8/8/8/K3R3 w - - 0 1");
    CHECK(attackersTo(pinned, Square::F5, Color::Black, sliding) == bit(Square::E7));
    const auto multiple = parse("k3r3/8/8/8/1b6/8/8/4K3 w - - 0 1");
    CHECK(attackersTo(multiple, Square::E1, Color::Black, sliding) == (bit(Square::E8) | bit(Square::B4)));
    const auto queen = parse("k7/8/8/8/7q/8/8/4K3 w - - 0 1");
    CHECK(inCheck(queen, Color::White, sliding));
    const auto kings = parse("8/8/8/8/8/8/4k3/4K3 w - - 0 1");
    CHECK(inCheck(kings, Color::White, sliding) && inCheck(kings, Color::Black, sliding));
}
}
int main(int argc, char** argv) {
    return test::run(argc, argv, std::array{
        test::Case{"leapers", leapers}, test::Case{"rays", rays},
        test::Case{"tables", tables}, test::Case{"queries", queries}});
}
