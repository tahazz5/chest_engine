#include "board/attacks.hpp"
#include <array>
#include <chrono>
#include <iomanip>
#include <iostream>

int main() {
    using namespace chess;
    using namespace chess::attacks;
    using Clock = std::chrono::steady_clock;
    struct Sample { Bitboard occupied; Square square; };
    std::array<Sample, 4096> samples{};
    std::uint64_t state = 0x853C49E6748FEA9BULL;
    for (auto& sample : samples) {
        state ^= state << 13U; state ^= state >> 7U; state ^= state << 17U;
        sample = {state, static_cast<Square>((state >> 32U) & 63U)};
    }
    constexpr unsigned Repeats = 128;
    std::cout << "hardware PEXT available: " << hardwarePextAvailable() << '\n'
              << "sizeof/alignof SlidingAttacks: " << sizeof(SlidingAttacks) << '/' << alignof(SlidingAttacks) << '\n'
              << "Each query computes rook | bishop; fixed pseudo-random occupancy corpus.\n";
    Bitboard referenceChecksum = 0;
    for (const auto mode : {SlidingBackend::Rays, SlidingBackend::PortableTable, SlidingBackend::PextTable}) {
        const auto initStart = Clock::now();
        const SlidingAttacks sliding(mode);
        const auto initEnd = Clock::now();
        Bitboard warmup = 0;
        for (const auto& sample : samples) warmup += sliding.queen(sample.square, sample.occupied);
        Bitboard checksum = 0;
        const auto start = Clock::now();
        for (unsigned repeat = 0; repeat < Repeats; ++repeat)
            for (const auto& sample : samples) checksum += sliding.queen(sample.square, sample.occupied);
        const double seconds = std::chrono::duration<double>(Clock::now() - start).count();
        const double initMs = std::chrono::duration<double, std::milli>(initEnd - initStart).count();
        const auto actual = sliding.backend();
        const char* name = actual == SlidingBackend::Rays ? "rays" : actual == SlidingBackend::PortableTable ? "portable table" : "PEXT table";
        if (mode == SlidingBackend::Rays) referenceChecksum = checksum;
        if (checksum != referenceChecksum || checksum != warmup * Repeats) {
            std::cerr << "Checksum mismatch\n"; return 1;
        }
        std::cout << name << ": init_ms=" << initMs << " table_bytes=" << sliding.tableBytes()
                  << " queries=" << samples.size() * Repeats << " elapsed_s=" << seconds
                  << " Mqueries/s=" << static_cast<double>(samples.size() * Repeats) / seconds / 1e6
                  << " checksum=" << std::hex << checksum << std::dec << '\n';
    }
}
