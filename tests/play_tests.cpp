#include "play/play.hpp"
#include "test_support.hpp"
#include <sstream>
#include <algorithm>
using namespace chess;
namespace {
Position parse(std::string_view fen) { const auto p = Position::fromFen(fen); CHECK(p); return *p; }
std::string session(std::string_view commands, PlayMode mode = PlayMode::Local, Position p = Position{}) {
    std::istringstream in{std::string(commands)}; std::ostringstream out;
    CHECK(play(in, out, mode, p) == 0); return out.str();
}
void interaction() {
    const auto text = session("e2e5\ncoups\ne2e4\nundo\nfen\nquit\n", PlayMode::White);
    CHECK(text.find("Coup illegal") != std::string::npos);
    CHECK(text.find("e2e4") != std::string::npos);
    CHECK(text.find("Ordinateur :") != std::string::npos);
    CHECK(text.find("Tour annule") != std::string::npos);
    CHECK(text.find(Position::StartFen) != std::string::npos);
    CHECK(text.find("Partie fermee") != std::string::npos);
    CHECK(session("quit\n", PlayMode::Black).find("Ordinateur :") != std::string::npos);
    CHECK(session("").find("Blancs >") != std::string::npos); // clean EOF
    const auto promotion = session("a7a8\na7a8q\nfen\nquit\n", PlayMode::Local, parse("7k/P7/8/8/8/8/8/4K3 w - - 0 1"));
    CHECK(promotion.find("Coup illegal") != std::string::npos);
    CHECK(promotion.find("Q6k/8/8/8/8/8/8/4K3 b - - 0 1") != std::string::npos);
}
void endings() {
    CHECK(session("f2f3\ne7e5\ng2g4\nd8h4\n").find("Echec et mat. Les noirs gagnent.") != std::string::npos);
    CHECK(session("", PlayMode::Local, parse("7k/5K2/6Q1/8/8/8/8/8 b - - 0 1")).find("Pat") != std::string::npos);
    CHECK(session("", PlayMode::Local, parse("7k/8/8/8/8/8/8/4K3 w - - 0 1")).find("materiel insuffisant") != std::string::npos);
    CHECK(session("g1f3\ng8f6\nf3g1\nf6g8\ng1f3\ng8f6\nf3g1\nf6g8\n").find("trois repetitions") != std::string::npos);
    CHECK(session("", PlayMode::Local, parse("7k/8/8/8/8/8/8/R3K3 w - - 100 90")).find("50 coups") != std::string::npos);
}
void opponent() {
    const attacks::SlidingAttacks sliding;
    Position p; const auto before = p;
    const auto chosen = chooseSimpleMove(p, sliding);
    MoveList legal; generateLegal(p, sliding, legal);
    CHECK(std::find(legal.begin(), legal.end(), chosen) != legal.end());
    CHECK(p == before);
    p = parse("7k/8/5KQ1/8/8/8/8/8 w - - 0 1");
    const auto mate = chooseSimpleMove(p, sliding);
    CHECK(!mate.isNone());
    const auto undo = p.makeMove(mate);
    generateLegal(p, sliding, legal);
    CHECK(legal.empty() && attacks::inCheck(p, Color::Black, sliding));
    p.unmakeMove(mate, undo);
    p = parse("7k/6Q1/5K2/8/8/8/8/8 b - - 0 1");
    CHECK(chooseSimpleMove(p, sliding).isNone());
}
}
int main(int argc, char** argv) {
    return test::run(argc, argv, std::array{test::Case{"interaction", interaction}, test::Case{"endings", endings}, test::Case{"opponent", opponent}});
}
