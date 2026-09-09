#include "board/attacks.hpp"
#include "board/position.hpp"

// Keep BMI2 confined to a targeted function. Never compile the whole engine with -mbmi2.
#if !defined(CHESS_DISABLE_PEXT) && defined(__x86_64__) && (defined(__GNUC__) || defined(__clang__)) && !defined(_MSC_VER)
#define CHESS_TARGETED_PEXT 1
#include <immintrin.h>
#else
#define CHESS_TARGETED_PEXT 0
#endif

namespace chess::attacks {
namespace {
constexpr std::array<detail::Step, 4> RookSteps{{{1, 0}, {-1, 0}, {0, 1}, {0, -1}}};
constexpr std::array<detail::Step, 4> BishopSteps{{{1, 1}, {-1, 1}, {1, -1}, {-1, -1}}};

Bitboard trace(Square square, Bitboard occupied, const std::array<detail::Step, 4>& steps,
               bool relevantOnly) noexcept {
    const int originFile = static_cast<int>(fileOf(square));
    const int originRank = static_cast<int>(rankOf(square));
    Bitboard result = 0;
    for (const auto step : steps) {
        int file = originFile + step.file, rank = originRank + step.rank;
        for (auto target = makeSquare(file, rank); isValid(target); target = makeSquare(file, rank)) {
            if (relevantOnly && !isValid(makeSquare(file + step.file, rank + step.rank))) break;
            result |= bit(target);
            if ((occupied & bit(target)) != 0) break;
            file += step.file; rank += step.rank;
        }
    }
    return result;
}

#if CHESS_TARGETED_PEXT
__attribute__((target("bmi2"), noinline))
std::uint64_t hardwareExtract(Bitboard occupied, Bitboard mask) noexcept {
    return _pext_u64(occupied, mask);
}
#endif
std::uint64_t lookupIndex(Bitboard occupied, Bitboard mask, SlidingBackend backend) noexcept {
#if CHESS_TARGETED_PEXT
    if (backend == SlidingBackend::PextTable) return hardwareExtract(occupied, mask);
#else
    (void)backend;
#endif
    return extractBits(occupied, mask);
}
}

Bitboard rookRays(Square square, Bitboard occupancy) noexcept { return trace(square, occupancy, RookSteps, false); }
Bitboard bishopRays(Square square, Bitboard occupancy) noexcept { return trace(square, occupancy, BishopSteps, false); }
Bitboard rookRelevantMask(Square square) noexcept { return trace(square, 0, RookSteps, true); }
Bitboard bishopRelevantMask(Square square) noexcept { return trace(square, 0, BishopSteps, true); }

bool hardwarePextAvailable() noexcept {
#if CHESS_TARGETED_PEXT
    __builtin_cpu_init();
    return __builtin_cpu_supports("bmi2") != 0;
#else
    return false;
#endif
}

struct SlidingAttacks::Tables {
    struct Entry {
        Bitboard mask = 0;
        std::uint32_t offset = 0;
    };
    std::array<Entry, 64> rooks{};
    std::array<Entry, 64> bishops{};
    std::array<Bitboard, RookEntries + BishopEntries> attacks{};

    Tables() noexcept {
        std::size_t offset = 0;
        for (const bool bishop : {false, true}) {
            for (unsigned s = 0; s < 64; ++s) {
                const auto square = static_cast<Square>(s);
                const auto mask = bishop ? bishopRelevantMask(square) : rookRelevantMask(square);
                auto& entry = bishop ? bishops[s] : rooks[s];
                entry = {mask, static_cast<std::uint32_t>(offset)};
                const std::size_t entries = std::size_t{1} << count(mask);
                assert(offset + entries <= attacks.size());
                Bitboard subset = 0;
                do {
                    const auto i = static_cast<std::size_t>(extractBits(subset, mask));
                    attacks[offset + i] = bishop ? bishopRays(square, subset) : rookRays(square, subset);
                    subset = (subset - mask) & mask; // enumerate every subset, unsigned arithmetic
                } while (subset != 0);
                offset += entries;
            }
            assert(offset == (bishop ? attacks.size() : RookEntries));
        }
    }
};

SlidingAttacks::SlidingAttacks(SlidingBackend requested) : backend_(requested) {
    assert(requested == SlidingBackend::Rays || requested == SlidingBackend::PortableTable ||
           requested == SlidingBackend::PextTable || requested == SlidingBackend::Auto);
    if (requested == SlidingBackend::Auto)
        backend_ = hardwarePextAvailable() ? SlidingBackend::PextTable : SlidingBackend::Rays;
    else if (requested == SlidingBackend::PextTable && !hardwarePextAvailable())
        backend_ = SlidingBackend::PortableTable;
    if (backend_ != SlidingBackend::Rays) tables_ = std::make_unique<Tables>();
}
SlidingAttacks::~SlidingAttacks() = default;
std::size_t SlidingAttacks::tableBytes() const noexcept { return tables_ ? sizeof(Tables) : 0; }
Bitboard SlidingAttacks::rook(Square square, Bitboard occupancy) const noexcept {
    if (backend_ == SlidingBackend::Rays) return rookRays(square, occupancy);
    const auto& entry = tables_->rooks[index(square)];
    return tables_->attacks[entry.offset + static_cast<std::size_t>(lookupIndex(occupancy, entry.mask, backend_))];
}
Bitboard SlidingAttacks::bishop(Square square, Bitboard occupancy) const noexcept {
    if (backend_ == SlidingBackend::Rays) return bishopRays(square, occupancy);
    const auto& entry = tables_->bishops[index(square)];
    return tables_->attacks[entry.offset + static_cast<std::size_t>(lookupIndex(occupancy, entry.mask, backend_))];
}

Bitboard attackersTo(const Position& position, Square target, Color byColor,
                     const SlidingAttacks& sliding) noexcept {
    const auto occupied = position.occupancy();
    // Reverse pawn direction: targets of the opposite color are candidate origins.
    return (pawn(opposite(byColor), target) & position.pieces(byColor, PieceType::Pawn)) |
           (knight(target) & position.pieces(byColor, PieceType::Knight)) |
           (king(target) & position.pieces(byColor, PieceType::King)) |
           (sliding.bishop(target, occupied) & (position.pieces(byColor, PieceType::Bishop) |
                                              position.pieces(byColor, PieceType::Queen))) |
           (sliding.rook(target, occupied) & (position.pieces(byColor, PieceType::Rook) |
                                            position.pieces(byColor, PieceType::Queen)));
}
bool isSquareAttacked(const Position& position, Square target, Color byColor,
                      const SlidingAttacks& sliding) noexcept {
    return attackersTo(position, target, byColor, sliding) != 0;
}
bool inCheck(const Position& position, Color color, const SlidingAttacks& sliding) noexcept {
    return isSquareAttacked(position, position.kingSquare(color), opposite(color), sliding);
}
} // namespace chess::attacks
