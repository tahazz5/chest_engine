#include "play/play.hpp"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <istream>
#include <ostream>
#include <string_view>
#include <vector>

namespace chess {
namespace {
int material(const Position& p) noexcept {
    constexpr std::array values{0, 100, 320, 330, 500, 900, 0};
    int score = 0;
    for (unsigned s = 0; s < 64; ++s) {
        const auto square = static_cast<Square>(s);
        const auto piece = p.pieceAt(square);
        if (piece == Piece::None) continue;
        const auto type = typeOf(piece);
        int value = values[static_cast<unsigned>(type)];
        if (type == PieceType::Pawn || type == PieceType::Knight || type == PieceType::Bishop) {
            const int file = static_cast<int>(fileOf(square)), rank = static_cast<int>(rankOf(square));
            value += 3 * (14 - std::abs(2 * file - 7) - std::abs(2 * rank - 7));
        }
        score += colorOf(piece) == p.sideToMove() ? value : -value;
    }
    return score;
}
int basicSearch(Position& p, const attacks::SlidingAttacks& sliding, int depth, int ply) {
    MoveList moves;
    generateLegal(p, sliding, moves);
    if (moves.empty()) return attacks::inCheck(p, p.sideToMove(), sliding) ? -30000 + ply : 0;
    if (depth == 0) return material(p);
    int best = -32000;
    for (const auto move : moves) {
        const auto undo = p.makeMove(move);
        const int score = -basicSearch(p, sliding, depth - 1, ply + 1);
        p.unmakeMove(move, undo);
        best = std::max(best, score);
    }
    return best;
}
void printBoard(const Position& p, std::ostream& out, bool flipped) {
    constexpr std::string_view chars = ".PNBRQKpnbrqk";
    out << '\n';
    for (int row = 0; row < 8; ++row) {
        const int rank = flipped ? row : 7 - row;
        out << rank + 1 << "  ";
        for (int col = 0; col < 8; ++col) {
            const int file = flipped ? 7 - col : col;
            out << chars[static_cast<unsigned>(p.pieceAt(makeSquare(file, rank)))] << ' ';
        }
        out << '\n';
    }
    out << (flipped ? "   h g f e d c b a\n" : "   a b c d e f g h\n");
}
std::string repetitionKey(const Position& p, const MoveList& legal) {
    auto fen = p.toFen();
    const auto epStart = fen.find(' ', fen.find(' ', fen.find(' ') + 1) + 1) + 1;
    const auto epEnd = fen.find(' ', epStart);
    fen.erase(epEnd); // exclude counters
    // En passant changes legal rights only if a legal capture exists.
    bool hasEp = false;
    for (const auto move : legal) if (move.isEnPassant()) hasEp = true;
    if (!hasEp) fen.replace(epStart, std::string::npos, "-");
    return fen;
}
bool insufficient(const Position& p) noexcept {
    if ((p.pieces(PieceType::Pawn) | p.pieces(PieceType::Rook) | p.pieces(PieceType::Queen)) != 0) return false;
    const auto minors = p.pieces(PieceType::Bishop) | p.pieces(PieceType::Knight);
    if (count(minors) <= 1) return true;
    if (p.pieces(PieceType::Knight) != 0) return false;
    auto bishops = p.pieces(PieceType::Bishop);
    const auto first = popLeastSquare(bishops);
    const auto shade = (fileOf(first) + rankOf(first)) % 2U;
    while (bishops != 0) {
        const auto square = popLeastSquare(bishops);
        if ((fileOf(square) + rankOf(square)) % 2U != shade) return false;
    }
    return true;
}
std::string trim(std::string text) {
    const auto begin = text.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) return {};
    return text.substr(begin, text.find_last_not_of(" \t\r\n") - begin + 1);
}
}

std::string moveText(Move move) {
    if (move.isNone()) return "0000";
    const auto from = squareName(move.from()), to = squareName(move.to());
    std::string text(from.data(), from.size());
    text.append(to.data(), to.size());
    if (move.isPromotion()) text += std::string_view("  nbrq ")[static_cast<unsigned>(move.promotedPiece())];
    return text;
}
Move chooseSimpleMove(Position& p, const attacks::SlidingAttacks& sliding) {
    MoveList moves;
    generateLegal(p, sliding, moves);
    Move best;
    int bestScore = -32000;
    for (const auto move : moves) {
        const auto undo = p.makeMove(move);
        const int score = -basicSearch(p, sliding, 1, 1); // our move plus one opponent reply
        p.unmakeMove(move, undo);
        if (score > bestScore) { bestScore = score; best = move; }
    }
    return best;
}

int play(std::istream& input, std::ostream& out, PlayMode mode, Position position) {
    const attacks::SlidingAttacks sliding(attacks::SlidingBackend::Auto);
    struct Played { Move move; UndoState undo; };
    std::vector<Played> history;
    std::vector<std::string> positions;
    out << "Partie d'echecs — " << (mode == PlayMode::Local ? "deux joueurs locaux" : mode == PlayMode::White ? "tu joues les blancs" : "tu joues les noirs")
        << ".\nMajuscules = blancs, minuscules = noirs. P pion, N cavalier, B fou, R tour, Q dame, K roi.\n"
        << "Saisis e2e4, e1g1 pour roquer, a7a8q pour promouvoir (n/b/r/q).\n"
        << "Commandes : coups, fen, undo, aide, abandon, quit.\n"
        << "Nulles par 3 repetitions ou 50 coups : revendication automatique dans ce mode.\n";
    MoveList legal;
    generateLegal(position, sliding, legal);
    positions.push_back(repetitionKey(position, legal));
    for (;;) {
        printBoard(position, out, mode == PlayMode::Black);
        const auto us = position.sideToMove();
        const bool check = attacks::inCheck(position, us, sliding);
        if (legal.empty()) {
            if (check) out << "Echec et mat. " << (us == Color::White ? "Les noirs" : "Les blancs") << " gagnent.\n";
            else out << "Pat : partie nulle.\n";
            return 0;
        }
        if (insufficient(position)) { out << "Nulle : materiel insuffisant.\n"; return 0; }
        if (position.halfmoveClock() >= 100) { out << "Nulle : regle des 50 coups.\n"; return 0; }
        if (std::count(positions.begin(), positions.end(), positions.back()) >= 3) {
            out << "Nulle : trois repetitions.\n"; return 0;
        }
        if (check) out << "Echec !\n";
        const bool human = mode == PlayMode::Local || us == (mode == PlayMode::White ? Color::White : Color::Black);
        Move selected;
        if (!human) {
            selected = chooseSimpleMove(position, sliding);
            out << "Ordinateur : " << moveText(selected) << '\n';
        } else {
            out << (us == Color::White ? "Blancs" : "Noirs") << " > " << std::flush;
            std::string text;
            if (!std::getline(input, text)) return 0;
            text = trim(text);
            if (text == "quit" || text == "exit") { out << "Partie fermee.\n"; return 0; }
            if (text == "abandon") { out << "Abandon. " << (us == Color::White ? "Les noirs" : "Les blancs") << " gagnent.\n"; return 0; }
            if (text == "fen") { out << position.toFen() << '\n'; continue; }
            if (text == "aide" || text == "help") {
                out << "Coup : e2e4 ; promotion : a7a8q (n/b/r/q) ; roque : e1g1 ou e1c1.\n"
                    << "coups : coups autorises ; fen : position ; undo : annuler le dernier tour ; abandon ; quit.\n";
                continue;
            }
            if (text == "coups" || text == "moves") {
                for (const auto move : legal) out << moveText(move) << ' ';
                out << '\n'; continue;
            }
            if (text == "undo") {
                const std::size_t amount = mode == PlayMode::Local ? 1 : 2;
                if (history.size() < amount) { out << "Aucun tour complet a annuler.\n"; continue; }
                for (std::size_t i = 0; i < amount; ++i) {
                    position.unmakeMove(history.back().move, history.back().undo);
                    history.pop_back(); positions.pop_back();
                }
                generateLegal(position, sliding, legal);
                out << "Tour annule.\n"; continue;
            }
            for (const auto move : legal) if (moveText(move) == text) { selected = move; break; }
            if (selected.isNone()) { out << "Coup illegal ou format inconnu. Tape coups pour voir les possibilites.\n"; continue; }
        }
        const auto undo = position.makeMove(selected);
        history.push_back({selected, undo});
        generateLegal(position, sliding, legal);
        positions.push_back(repetitionKey(position, legal));
    }
}
} // namespace chess
