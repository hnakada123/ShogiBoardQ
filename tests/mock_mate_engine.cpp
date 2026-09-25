// 自動化・CLI・MCP の結合テスト用の最小 USI 詰将棋エンジン。
// `go mate <ms>` に Hayanagi の詰み探索で答える。棋力の検証には使わない。
#include "position.h"
#include "tsume.h"

#include <atomic>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::vector<std::string> tokenize(const std::string& line)
{
    std::istringstream stream(line);
    std::vector<std::string> tokens;
    for (std::string token; stream >> token;) tokens.push_back(token);
    return tokens;
}

bool applyPosition(const std::vector<std::string>& tokens, shogi::Position& position)
{
    std::size_t i = 1;
    if (i < tokens.size() && tokens[i] == "startpos") {
        position.set_startpos();
        ++i;
    } else if (i < tokens.size() && tokens[i] == "sfen") {
        // sfen <board> <turn> <hands> [<move number>] [moves ...]
        std::string sfen;
        std::size_t fields = 0;
        for (++i; i < tokens.size() && tokens[i] != "moves" && fields < 4; ++i, ++fields) {
            if (!sfen.empty()) sfen += ' ';
            sfen += tokens[i];
        }
        if (fields == 3) sfen += " 1";
        if (!position.set_sfen(sfen, true)) return false;
    } else {
        return false;
    }
    if (i < tokens.size() && tokens[i] == "moves") {
        for (std::size_t k = i + 1; k < tokens.size(); ++k) {
            if (!position.apply_usi_move(tokens[k])) return false;
        }
    }
    return true;
}

std::string solveMate(const shogi::Position& root, int timeLimitMs)
{
    shogi::Position position = root;
    const auto attacker = position.side_to_move();
    shogi::TsumeSearch search;
    std::atomic_bool stop{false};
    auto result = search.solve(position, attacker, 15, timeLimitMs, stop);
    if (result.status == shogi::TsumeStatus::NoMate) return "checkmate nomate";
    if (result.status != shogi::TsumeStatus::Mate) return "checkmate timeout";
    std::string pv;
    for (int remaining = result.plies; remaining > 0; --remaining) {
        result = search.solve(position, attacker, remaining, timeLimitMs, stop);
        if (result.status != shogi::TsumeStatus::Mate || !result.move.is_valid()) return "checkmate timeout";
        pv += ' ' + position.move_to_usi(result.move);
        position.do_move(result.move);
    }
    return "checkmate" + pv;
}

} // namespace

int main()
{
    shogi::Position position;
    position.set_startpos();
    for (std::string line; std::getline(std::cin, line);) {
        const auto tokens = tokenize(line);
        if (tokens.empty()) continue;
        const std::string& command = tokens[0];
        if (command == "usi") {
            std::cout << "id name MockMate\nid author ShogiBoardQ tests\n"
                      << "option name USI_Hash type spin default 16 min 1 max 1024\n"
                      << "usiok" << std::endl;
        } else if (command == "isready") {
            std::cout << "readyok" << std::endl;
        } else if (command == "position") {
            if (!applyPosition(tokens, position)) std::cout << "info string invalid position" << std::endl;
        } else if (command == "go") {
            if (tokens.size() >= 2 && tokens[1] == "mate") {
                int timeLimitMs = 1000;
                if (tokens.size() >= 3 && tokens[2] != "infinite") timeLimitMs = std::stoi(tokens[2]);
                std::cout << solveMate(position, timeLimitMs) << std::endl;
            } else {
                // 通常探索は持たない。解析の結合テストは Hayanagi 本体を使う
                std::cout << "info depth 1 score cp 0 pv resign\nbestmove resign" << std::endl;
            }
        } else if (command == "quit") {
            return 0;
        }
        // setoption / usinewgame / stop / gameover は無視する
    }
    return 0;
}
