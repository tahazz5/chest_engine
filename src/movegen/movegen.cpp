#include "movegen/movegen.hpp"

namespace chess {
namespace {
void pawnMove(MoveList& output, Square from, Square to, bool capture, unsigned promotionRank) noexcept {
    if (rankOf(to) == promotionRank) {
        constexpr std::array quiet{MoveFlag::PromoteKnight, MoveFlag::PromoteBishop,
                                   MoveFlag::PromoteRook, MoveFlag::PromoteQueen};
        constexpr std::array captures{MoveFlag::CapturePromoteKnight, MoveFlag::CapturePromoteBishop,
                                      MoveFlag::CapturePromoteRook, MoveFlag::CapturePromoteQueen};
        for (const auto flag : capture ? captures : quiet) output.push(Move(from, to, flag));
    } else output.push(Move(from, to, capture ? MoveFlag::Capture : MoveFlag::Quiet));
}
void castles(const Position& p, const attacks::SlidingAttacks& sliding, MoveList& output) noexcept {
    const auto us = p.sideToMove(), them = opposite(us);
    const int rank = us == Color::White ? 0 : 7;
    const auto king = makeSquare(4, rank);
    if (p.pieceAt(king) != makePiece(us, PieceType::King)) return;
    const auto kingRight = us == Color::White ? CastlingRights::WhiteKingSide : CastlingRights::BlackKingSide;
    const auto queenRight = us == Color::White ? CastlingRights::WhiteQueenSide : CastlingRights::BlackQueenSide;
    if (!hasRight(p.castlingRights(), kingRight) && !hasRight(p.castlingRights(), queenRight)) return;
    if (attacks::isSquareAttacked(p, king, them, sliding)) return;
    for (const bool kingSide : {true, false}) {
        if (!hasRight(p.castlingRights(), kingSide ? kingRight : queenRight)) continue;
        const auto rook = makeSquare(kingSide ? 7 : 0, rank);
        if (p.pieceAt(rook) != makePiece(us, PieceType::Rook)) continue;
        const auto transit = makeSquare(kingSide ? 5 : 3, rank);
        const auto destination = makeSquare(kingSide ? 6 : 2, rank);
        Bitboard emptyPath = bit(transit) | bit(destination);
        if (!kingSide) emptyPath |= bit(makeSquare(1, rank));
        if ((p.occupancy() & emptyPath) != 0) continue;
        if (attacks::isSquareAttacked(p, transit, them, sliding) ||
            attacks::isSquareAttacked(p, destination, them, sliding)) continue;
        output.push(Move(king, destination, kingSide ? MoveFlag::KingCastle : MoveFlag::QueenCastle));
    }
}
}
void generatePseudoLegal(const Position& p, const attacks::SlidingAttacks& sliding, MoveList& output) noexcept {
    assert(p.isConsistent());
    output.clear();
    const auto us = p.sideToMove(), them = opposite(us);
    const auto occupied = p.occupancy();
    const auto capturable = p.pieces(them) & ~p.pieces(them, PieceType::King);
    const auto destinations = ~p.pieces(us) & ~p.pieces(them, PieceType::King);
    const int forward = us == Color::White ? 1 : -1;
    const unsigned startRank = us == Color::White ? 1U : 6U;
    const unsigned promotionRank = us == Color::White ? 7U : 0U;
    auto pawns = p.pieces(us, PieceType::Pawn);
    while (pawns != 0) {
        const auto from = popLeastSquare(pawns);
        const int file = static_cast<int>(fileOf(from)), rank = static_cast<int>(rankOf(from));
        const auto single = makeSquare(file, rank + forward);
        if (isValid(single) && !contains(occupied, single)) {
            pawnMove(output, from, single, false, promotionRank);
            if (rankOf(from) == startRank) {
                const auto twice = makeSquare(file, rank + 2 * forward);
                if (!contains(occupied, twice)) output.push(Move(from, twice, MoveFlag::DoublePawnPush));
            }
        }
        auto captures = attacks::pawn(us, from) & capturable;
        while (captures != 0) pawnMove(output, from, popLeastSquare(captures), true, promotionRank);
        const auto ep = p.enPassantSquare();
        if (isValid(ep) && contains(attacks::pawn(us, from), ep))
            output.push(Move(from, ep, MoveFlag::EnPassant));
    }
    for (const auto type : {PieceType::Knight, PieceType::Bishop, PieceType::Rook, PieceType::Queen, PieceType::King}) {
        auto pieces = p.pieces(us, type);
        while (pieces != 0) {
            const auto from = popLeastSquare(pieces);
            Bitboard targets = 0;
            switch (type) {
                case PieceType::Knight: targets = attacks::knight(from); break;
                case PieceType::Bishop: targets = sliding.bishop(from, occupied); break;
                case PieceType::Rook: targets = sliding.rook(from, occupied); break;
                case PieceType::Queen: targets = sliding.queen(from, occupied); break;
                case PieceType::King: targets = attacks::king(from); break;
                default: break;
            }
            targets &= destinations;
            while (targets != 0) {
                const auto to = popLeastSquare(targets);
                output.push(Move(from, to, contains(capturable, to) ? MoveFlag::Capture : MoveFlag::Quiet));
            }
        }
    }
    castles(p, sliding, output);
}
} // namespace chess

namespace chess {
void generateLegal(Position& p, const attacks::SlidingAttacks& sliding, MoveList& output) noexcept {
    MoveList candidates;
    generatePseudoLegal(p, sliding, candidates);
    output.clear();
    const auto us = p.sideToMove();
    for (const auto move : candidates) {
        const auto undo = p.makeMove(move);
        const bool legal = !attacks::inCheck(p, us, sliding);
        p.unmakeMove(move, undo);
        if (legal) output.push(move);
    }
}
} // namespace chess
