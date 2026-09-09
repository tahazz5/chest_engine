#include "board/move.hpp"
#include "board/attacks.hpp"
#include "board/position.hpp"
#include "movegen/movegen.hpp"
#include "play/play.hpp"
#include <iostream>
#include <string>
#include <string_view>

int main(int argc, char** argv) {
    if (argc >= 2 && std::string_view(argv[1]) == "--play") {
        chess::PlayMode mode = chess::PlayMode::White;
        if (argc > 3) { std::cerr << "Usage: chess_engine --play [white|black|local]\n"; return 1; }
        if (argc == 3) {
            const std::string_view choice(argv[2]);
            if (choice == "black") mode = chess::PlayMode::Black;
            else if (choice == "local") mode = chess::PlayMode::Local;
            else if (choice != "white") { std::cerr << "Choisir white, black ou local.\n"; return 1; }
        }
        return chess::play(std::cin, std::cout, mode);
    }
    if (argc > 1) {
        if (argc != 3 || (std::string_view(argv[1]) != "--fen" && std::string_view(argv[1]) != "--pseudo-moves")) {
            std::cerr << "Usage: chess_engine --play [white|black|local]\n"
                      << "       chess_engine [--fen|--pseudo-moves \"<six FEN fields>\"]\n";
            return 1;
        }
        std::string error;
        const auto position = chess::Position::fromFen(argv[2], &error);
        if (!position) { std::cerr << "Invalid FEN: " << error << '\n'; return 1; }
        if (std::string_view(argv[1]) == "--pseudo-moves") {
            const chess::attacks::SlidingAttacks sliding;
            chess::MoveList moves;
            chess::generatePseudoLegal(*position, sliding, moves);
            std::cout << "Pseudo-legal candidates (king safety not fully filtered): " << moves.size() << '\n';
            for (const auto move : moves) {
                for (const char c : chess::squareName(move.from())) std::cout << c;
                for (const char c : chess::squareName(move.to())) std::cout << c;
                if (move.isPromotion())
                    std::cout << std::string_view("  nbrq ")[static_cast<unsigned>(move.promotedPiece())];
                std::cout << '\n';
            }
            return 0;
        }
        std::cout << position->toFen() << '\n';
        return 0;
    }
    std::cout << "Chest Engine — mode jouable : lancer avec --play\n"
              << "sizeof / alignof, en octets:\n"
              << "Color: " << sizeof(chess::Color) << " / " << alignof(chess::Color) << '\n'
              << "Piece: " << sizeof(chess::Piece) << " / " << alignof(chess::Piece) << '\n'
              << "Square: " << sizeof(chess::Square) << " / " << alignof(chess::Square) << '\n'
              << "Move: " << sizeof(chess::Move) << " / " << alignof(chess::Move) << '\n'
              << "MoveList: " << sizeof(chess::MoveList) << " / " << alignof(chess::MoveList) << '\n'
              << "Bitboard: " << sizeof(chess::Bitboard) << " / " << alignof(chess::Bitboard) << '\n'
              << "GameState: " << sizeof(chess::GameState) << " / " << alignof(chess::GameState) << '\n'
              << "Position: " << sizeof(chess::Position) << " / " << alignof(chess::Position) << '\n'
              << "SlidingAttacks: " << sizeof(chess::attacks::SlidingAttacks) << " / " << alignof(chess::attacks::SlidingAttacks) << '\n'
              << "Hardware PEXT available: " << chess::attacks::hardwarePextAvailable() << '\n'
              << "Initial FEN: " << chess::Position{}.toFen() << '\n';
}
