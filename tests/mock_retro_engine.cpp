// 逆算・厳密証明テスト用の KomoringHeights 互換モック詰将棋エンジン。
// KomoringHeights 必須オプションの受信を検証し、Hayanagi の有限深さ探索で詰み/不詰を返す。
#include "position.h"
#include "tsume.h"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <thread>
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

std::string solveMate(const shogi::Position& root, int maxPlies, int timeLimitMs)
{
    shogi::Position position = root;
    const auto attacker = position.side_to_move();
    shogi::TsumeSearch search;
    std::atomic_bool stop{false};
    auto result = search.solve(position, attacker, maxPlies, timeLimitMs, stop);
    if (result.status == shogi::TsumeStatus::NoMate || result.status == shogi::TsumeStatus::Limit) {
        return "checkmate nomate";
    }
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
    const char* depthEnv = std::getenv("SHOGI_MOCK_RETRO_DEPTH");
    const int maxPlies = depthEnv ? std::max(1, std::atoi(depthEnv)) : 7;

    shogi::Position position;
    position.set_startpos();
    std::map<std::string, std::string> options;

    for (std::string line; std::getline(std::cin, line);) {
        const auto tokens = tokenize(line);
        if (tokens.empty()) continue;
        const std::string& command = tokens[0];
        if (command == "usi") {
            std::cout << "id name KomoringHeights MockRetro\n"
                      << "id author ShogiBoardQ tests\n"
                      << "option name PostSearchLevel type combo default None var None var MinLength\n"
                      << "option name GenerateAllLegalMoves type check default false\n"
                      << "option name RootIsAndNodeIfChecked type check default true\n"
                      << "option name NodesLimit type spin default 1000 min 0 max 1000000\n"
                      << "option name MultiPV type spin default 1 min 1 max 10\n"
                      << "option name USI_Hash type spin default 16 min 1 max 4096\n"
                      << "usiok" << std::endl;
        } else if (command == "setoption") {
            const auto namePos = line.find("name ");
            const auto valPos = line.find(" value ");
            if (namePos != std::string::npos && valPos != std::string::npos && valPos > namePos + 5) {
                options[line.substr(namePos + 5, valPos - (namePos + 5))] = line.substr(valPos + 7);
            }
        } else if (command == "isready") {
            std::cout << "readyok" << std::endl;
        } else if (command == "position") {
            if (!applyPosition(tokens, position)) std::cout << "info string invalid position" << std::endl;
        } else if (command == "go") {
            if (tokens.size() >= 2 && tokens[1] == "mate") {
                if (options["PostSearchLevel"] != "MinLength"
                    || options["GenerateAllLegalMoves"] != "true"
                    || options["RootIsAndNodeIfChecked"] != "false"
                    || options["NodesLimit"] != "0"
                    || options["MultiPV"] != "1") {
                    std::cout << "checkmate notimplemented" << std::endl;
                    continue;
                }
                int timeLimitMs = 200;
                if (tokens.size() >= 3 && tokens[2] != "infinite") {
                    timeLimitMs = std::max(10, std::min(500, std::stoi(tokens[2])));
                }
                std::cout << solveMate(position, maxPlies, timeLimitMs) << std::endl;
            } else {
                std::cout << "bestmove resign" << std::endl;
            }
        } else if (command == "quit") {
            return 0;
        }
    }
    return 0;
}
