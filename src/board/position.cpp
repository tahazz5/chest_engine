#include "board/position.hpp"
#include <charconv>
#include <limits>
#include <system_error>

namespace chess {
namespace {
constexpr std::string_view PieceChars = " PNBRQKpnbrqk";
constexpr unsigned pieceTypeIndex(Piece piece) noexcept {
    assert(isValid(piece) && piece != Piece::None);
    // Dense codes 1..6 and 7..12 map directly to indices 0..5.
    return (static_cast<unsigned>(piece) - 1U) % 6U;
}
constexpr bool whitespace(char c) noexcept {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}
bool parseCounter(std::string_view text, std::uint32_t& result) noexcept {
    if (text.empty() || text.front() < '0' || text.front() > '9') return false;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), result);
    return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size();
}
}

Position::Position() noexcept {
    constexpr std::array backRank{PieceType::Rook, PieceType::Knight, PieceType::Bishop,
        PieceType::Queen, PieceType::King, PieceType::Bishop, PieceType::Knight, PieceType::Rook};
    for (int file = 0; file < 8; ++file) {
        const auto type = backRank[static_cast<unsigned>(file)];
        putPiece(makePiece(Color::White, type), makeSquare(file, 0));
        putPiece(Piece::WhitePawn, makeSquare(file, 1));
        putPiece(Piece::BlackPawn, makeSquare(file, 6));
        putPiece(makePiece(Color::Black, type), makeSquare(file, 7));
    }
    state_.castlingRights = CastlingRights::All;
    assert(isConsistent());
}

void Position::putPiece(Piece piece, Square square) noexcept {
    assert(isValid(piece) && piece != Piece::None && isValid(square));
    assert(board_[index(square)] == Piece::None);
    board_[index(square)] = piece;
    byType_[pieceTypeIndex(piece)] |= bit(square);
    byColor_[static_cast<unsigned>(*colorOf(piece))] |= bit(square);
}

void Position::removePiece(Square square) noexcept {
    const auto piece = pieceAt(square);
    assert(piece != Piece::None);
    byType_[pieceTypeIndex(piece)] &= ~bit(square);
    byColor_[static_cast<unsigned>(*colorOf(piece))] &= ~bit(square);
    board_[index(square)] = Piece::None;
}

UndoState Position::makeMove(Move move) noexcept {
    assert(!move.isNone() && isConsistent());
    const auto from = move.from(), to = move.to();
    const auto moving = pieceAt(from);
    const auto us = sideToMove();
    assert(colorOf(moving) == us);
    const auto captureSquare = move.isEnPassant()
        ? makeSquare(static_cast<int>(fileOf(to)), static_cast<int>(rankOf(from))) : to;
    const UndoState undo{state_, pieceAt(captureSquare)};
    assert(typeOf(undo.captured) != PieceType::King);
    assert(move.isCapture() == (undo.captured != Piece::None));
    if (undo.captured != Piece::None) removePiece(captureSquare);
    removePiece(from);
    putPiece(move.isPromotion() ? makePiece(us, move.promotedPiece()) : moving, to);
    if (move.isCastling()) {
        const int rank = static_cast<int>(rankOf(from));
        const bool kingSide = move.flag() == MoveFlag::KingCastle;
        const auto rookFrom = makeSquare(kingSide ? 7 : 0, rank);
        const auto rookTo = makeSquare(kingSide ? 5 : 3, rank);
        removePiece(rookFrom);
        putPiece(makePiece(us, PieceType::Rook), rookTo);
    }
    unsigned rights = static_cast<unsigned>(state_.castlingRights);
    if (typeOf(moving) == PieceType::King) rights &= ~(us == Color::White ? 3U : 12U);
    constexpr std::array corners{Square::H1, Square::A1, Square::H8, Square::A8};
    for (unsigned i = 0; i < corners.size(); ++i)
        if (from == corners[i] || to == corners[i]) rights &= ~(1U << i);
    state_.castlingRights = static_cast<CastlingRights>(rights);
    state_.enPassantSquare = move.isDoublePawnPush()
        ? makeSquare(static_cast<int>(fileOf(from)), static_cast<int>((rankOf(from) + rankOf(to)) / 2U))
        : Square::None;
    if (typeOf(moving) == PieceType::Pawn || move.isCapture()) state_.halfmoveClock = 0;
    else if (state_.halfmoveClock != std::numeric_limits<std::uint32_t>::max()) ++state_.halfmoveClock;
    if (us == Color::Black && state_.fullmoveNumber != std::numeric_limits<std::uint32_t>::max()) ++state_.fullmoveNumber;
    state_.sideToMove = opposite(us);
    assert(isConsistent());
    return undo;
}

void Position::unmakeMove(Move move, const UndoState& undo) noexcept {
    const auto us = undo.previous.sideToMove;
    const auto from = move.from(), to = move.to();
    const auto moving = move.isPromotion() ? makePiece(us, PieceType::Pawn) : pieceAt(to);
    removePiece(to);
    if (move.isCastling()) {
        const int rank = static_cast<int>(rankOf(from));
        const bool kingSide = move.flag() == MoveFlag::KingCastle;
        removePiece(makeSquare(kingSide ? 5 : 3, rank));
        putPiece(makePiece(us, PieceType::Rook), makeSquare(kingSide ? 7 : 0, rank));
    }
    putPiece(moving, from);
    if (undo.captured != Piece::None) {
        const auto captureSquare = move.isEnPassant()
            ? makeSquare(static_cast<int>(fileOf(to)), static_cast<int>(rankOf(from))) : to;
        putPiece(undo.captured, captureSquare);
    }
    state_ = undo.previous;
    assert(isConsistent());
}

std::optional<Position> Position::fromFen(std::string_view fen, std::string* error) {
    // Own the diagnostic before assignment: even an input view into *error is safe.
    const auto fail = [error](const char* message) -> std::optional<Position> {
        if (error) *error = message;
        return std::nullopt;
    };
    std::array<std::string_view, 6> fields{};
    std::size_t cursor = 0;
    for (auto& field : fields) {
        while (cursor < fen.size() && whitespace(fen[cursor])) ++cursor;
        const auto start = cursor;
        while (cursor < fen.size() && !whitespace(fen[cursor])) ++cursor;
        if (cursor == start) return fail("FEN must contain exactly six fields");
        field = fen.substr(start, cursor - start);
    }
    while (cursor < fen.size() && whitespace(fen[cursor])) ++cursor;
    if (cursor != fen.size()) return fail("Unexpected field after fullmove number");

    Position result(EmptyTag{});
    int rank = 7, file = 0;
    bool previousDigit = false;
    for (const char c : fields[0]) {
        if (c == '/') {
            if (file != 8 || rank == 0) return fail("Each of the eight ranks must contain eight squares");
            --rank; file = 0; previousDigit = false;
        } else if (c >= '1' && c <= '8') {
            if (previousDigit) return fail("Adjacent empty-square digits are not canonical FEN");
            file += c - '0';
            if (file > 8) return fail("Rank contains more than eight squares");
            previousDigit = true;
        } else {
            const auto code = PieceChars.find(c);
            if (code == std::string_view::npos || code == 0 || file >= 8)
                return fail("Invalid piece or rank width");
            result.putPiece(static_cast<Piece>(code), makeSquare(file, rank));
            ++file; previousDigit = false;
        }
    }
    if (rank != 0 || file != 8) return fail("Piece placement must describe eight complete ranks");

    if (fields[1] == "w") result.state_.sideToMove = Color::White;
    else if (fields[1] == "b") result.state_.sideToMove = Color::Black;
    else return fail("Side to move must be w or b");

    if (fields[2] != "-") {
        for (const char c : fields[2]) {
            const auto i = std::string_view("KQkq").find(c);
            if (i == std::string_view::npos) return fail("Invalid castling right");
            const auto right = static_cast<CastlingRights>(1U << i);
            if (hasRight(result.state_.castlingRights, right)) return fail("Duplicate castling right");
            result.state_.castlingRights = result.state_.castlingRights | right;
        }
    }
    if (fields[3] != "-") {
        result.state_.enPassantSquare = parseSquare(fields[3]);
        if (!isValid(result.state_.enPassantSquare)) return fail("Invalid en passant square");
    }
    if (!parseCounter(fields[4], result.state_.halfmoveClock)) return fail("Invalid or overflowing halfmove clock");
    if (!parseCounter(fields[5], result.state_.fullmoveNumber) || result.state_.fullmoveNumber == 0)
        return fail("Fullmove number must be a positive uint32");

    if (const auto* reason = result.validationError()) return fail(reason);
    assert(result.isConsistent());
    if (error) error->clear();
    return result;
}

const char* Position::validationError() const noexcept {
    if (!isValid(state_.sideToMove) || state_.fullmoveNumber == 0 ||
        static_cast<unsigned>(state_.castlingRights) > 15)
        return "Invalid game state";
    std::array<Bitboard, 6> types{};
    std::array<Bitboard, 2> colors{};
    for (unsigned s = 0; s < 64; ++s) {
        const auto piece = board_[s];
        if (!isValid(piece)) return "Invalid piece code";
        if (piece == Piece::None) continue;
        const auto mask = bit(static_cast<Square>(s));
        types[pieceTypeIndex(piece)] |= mask;
        colors[static_cast<unsigned>(*colorOf(piece))] |= mask;
    }
    if (types != byType_ || colors != byColor_) return "Mailbox and bitboards disagree";
    for (const auto color : {Color::White, Color::Black}) {
        if (count(pieces(color, PieceType::King)) != 1) return "Exactly one king per color is required";
        if (count(pieces(color, PieceType::Pawn)) > 8 || count(pieces(color)) > 16)
            return "Too many pawns or pieces for one color";
    }
    if ((pieces(PieceType::Pawn) & (Rank1 | Rank8)) != 0) return "Pawn on first or eighth rank";

    constexpr std::array rights{CastlingRights::WhiteKingSide, CastlingRights::WhiteQueenSide,
        CastlingRights::BlackKingSide, CastlingRights::BlackQueenSide};
    constexpr std::array rookSquares{Square::H1, Square::A1, Square::H8, Square::A8};
    for (unsigned i = 0; i < rights.size(); ++i) {
        if (!hasRight(state_.castlingRights, rights[i])) continue;
        const auto color = i < 2 ? Color::White : Color::Black;
        const auto king = i < 2 ? Square::E1 : Square::E8;
        if (pieceAt(king) != makePiece(color, PieceType::King) ||
            pieceAt(rookSquares[i]) != makePiece(color, PieceType::Rook))
            return "Castling right requires king and rook on their original squares";
    }
    if (state_.enPassantSquare != Square::None) {
        const auto ep = state_.enPassantSquare;
        if (!isValid(ep)) return "Invalid en passant square";
        const bool whiteToMove = sideToMove() == Color::White;
        const int epRank = whiteToMove ? 5 : 2;
        if (static_cast<int>(rankOf(ep)) != epRank || pieceAt(ep) != Piece::None)
            return "En passant target must be empty and on the rank matching the side to move";
        const int file = static_cast<int>(fileOf(ep));
        const auto pawnSquare = makeSquare(file, whiteToMove ? 4 : 3);
        const auto origin = makeSquare(file, whiteToMove ? 6 : 1);
        if (pieceAt(pawnSquare) != makePiece(opposite(sideToMove()), PieceType::Pawn) ||
            pieceAt(origin) != Piece::None || halfmoveClock() != 0)
            return "En passant target requires a consistent last double pawn push";
        // FEN preserves a target after a double push even with no possible capturer.
    }
    return nullptr;
}

std::string Position::toFen() const {
    assert(isConsistent());
    std::string fen;
    fen.reserve(96);
    for (int rank = 7; rank >= 0; --rank) {
        unsigned empty = 0;
        for (int file = 0; file < 8; ++file) {
            const auto piece = pieceAt(makeSquare(file, rank));
            if (piece == Piece::None) { ++empty; continue; }
            if (empty != 0) { fen += static_cast<char>('0' + empty); empty = 0; }
            fen += PieceChars[static_cast<unsigned>(piece)];
        }
        if (empty != 0) fen += static_cast<char>('0' + empty);
        if (rank != 0) fen += '/';
    }
    fen += sideToMove() == Color::White ? " w " : " b ";
    constexpr std::array rights{CastlingRights::WhiteKingSide, CastlingRights::WhiteQueenSide,
        CastlingRights::BlackKingSide, CastlingRights::BlackQueenSide};
    if (castlingRights() == CastlingRights::None) fen += '-';
    else for (unsigned i = 0; i < rights.size(); ++i)
        if (hasRight(castlingRights(), rights[i])) fen += std::string_view("KQkq")[i];
    fen += ' ';
    if (enPassantSquare() == Square::None) fen += '-';
    else {
        const auto name = squareName(enPassantSquare());
        fen.append(name.data(), name.size());
    }
    fen += ' ';
    fen += std::to_string(halfmoveClock());
    fen += ' ';
    fen += std::to_string(fullmoveNumber());
    return fen;
}
} // namespace chess
