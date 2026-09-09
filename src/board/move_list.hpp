#pragma once
#include "board/move.hpp"
#include <array>
#include <cstdlib>
#include <span>

namespace chess {
class MoveList {
public:
    // Conservative bound for Position's <=16 pieces/color, including promoted pieces.
    inline static constexpr std::size_t Capacity = 512;
    [[nodiscard]] std::size_t size() const noexcept { return size_; }
    [[nodiscard]] bool empty() const noexcept { return size_ == 0; }
    void clear() noexcept { size_ = 0; }
    [[nodiscard]] bool tryPush(Move move) noexcept {
        if (move.isNone() || size_ == Capacity) return false;
        moves_[size_++] = move;
        return true;
    }
    void push(Move move) noexcept {
        // A broken capacity invariant must never silently truncate or corrupt memory.
        if (!tryPush(move)) std::abort();
    }
    [[nodiscard]] const Move& operator[](std::size_t i) const noexcept {
        assert(i < size_); return moves_[i];
    }
    [[nodiscard]] std::span<const Move> view() const noexcept { return {moves_.data(), size_}; }
    [[nodiscard]] const Move* begin() const noexcept { return moves_.data(); }
    [[nodiscard]] const Move* end() const noexcept { return moves_.data() + size_; }
private:
    std::array<Move, Capacity> moves_{};
    std::uint16_t size_ = 0;
};
static_assert(std::is_trivially_copyable_v<MoveList>);
} // namespace chess
