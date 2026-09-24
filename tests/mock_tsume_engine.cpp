// 外部エンジンのプロトコル・停止・異常応答の試験用。棋力の検証には使わない。
#include "tsume.h"
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <thread>

int main()
{
    const char* environment = std::getenv("SHOGI_TEST_MATE_MODE");
    const std::string mode = environment ? environment : "";
    std::map<std::string, std::string> options;
    std::string sfen;
    for (std::string line; std::getline(std::cin, line);) {
        if (line == "usi") {
            std::cout << "id name " << (mode == "unsupported" ? "Other" : "KomoringHeights mock") << '\n'
                      << "option name PostSearchLevel type combo default MinLength var None var MinLength\n"
                      << "option name GenerateAllLegalMoves type check default false\n"
                      << "option name RootIsAndNodeIfChecked type check default true\n"
                      << "option name NodesLimit type spin default 1 min 0 max 1000\n"
                      << "usiok" << std::endl;
        } else if (line.rfind("setoption name ", 0) == 0) {
            const auto split = line.find(" value ");
            options[line.substr(15, split - 15)] = line.substr(split + 7);
        } else if (line == "isready") std::cout << "readyok" << std::endl;
        else if (line.rfind("position sfen ", 0) == 0) sfen = line.substr(14);
        else if (line.rfind("go mate ", 0) == 0) {
            if (line != "go mate infinite" || options["PostSearchLevel"] != "MinLength"
                || options["GenerateAllLegalMoves"] != "true" || options["RootIsAndNodeIfChecked"] != "false"
                || options["NodesLimit"] != "0") {
                std::cout << "checkmate notimplemented" << std::endl;
                continue;
            }
            if (mode == "crash") return 2;
            if (mode == "slow") {
                // info の途中手順は証明ではない。停止後の遅延応答も採択してはいけない。
                std::cout << "info depth 1 pv R*1b" << std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(2));
            }
            if (mode == "malformed") { std::cout << "checkmate R*9i" << std::endl; continue; }
            if (mode == "timeout") { std::cout << "checkmate timeout" << std::endl; continue; }
            shogi::Position position;
            if (!position.set_sfen(sfen, true)) return 3;
            const auto attacker = position.side_to_move();
            shogi::TsumeSearch search;
            std::atomic_bool stop{false};
            auto result = search.solve(position, attacker, 9, 1000, stop);
            if (result.status == shogi::TsumeStatus::NoMate) { std::cout << "checkmate nomate" << std::endl; continue; }
            if (result.status != shogi::TsumeStatus::Mate) { std::cout << "checkmate timeout" << std::endl; continue; }
            std::ostringstream pv;
            bool valid = true;
            for (int remaining = result.plies; remaining > 0; --remaining) {
                result = search.solve(position, attacker, remaining, 1000, stop);
                if (result.status != shogi::TsumeStatus::Mate || !result.move.is_valid()) { valid = false; break; }
                pv << ' ' << position.move_to_usi(result.move);
                position.do_move(result.move);
            }
            std::cout << (valid ? "checkmate" + pv.str() : "checkmate timeout") << std::endl;
        } else if (line == "quit") return 0;
    }
}
