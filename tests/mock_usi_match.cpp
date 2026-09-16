// 対局通信の回帰テスト用。先読み中の二重goには応答しない。
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <cstdlib>

int main()
{
    bool pondering = false;
    bool ponderEnabled = true; // Gikou同様、未報告でも既定値はtrue
    const char* modeEnv = std::getenv("SBQ_MATCH_PONDER_MODE");
    const std::string mode = modeEnv ? modeEnv : "hidden";
    int searches = 0;
    int delay = 0;
    std::thread worker;
    auto join = [&] { if (worker.joinable()) worker.join(); };
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line == "usi") {
            if (mode == "reported") {
                std::cout << "option name USI_Ponder type check default true" << std::endl;
            }
            std::cout << "usiok" << std::endl;
        }
        else if (line == "isready") std::cout << "readyok" << std::endl;
        else if (line.find("setoption name USI_Ponder value ") == 0) {
            if (mode == "gui-only") { join(); return 1; }
            ponderEnabled = line == "setoption name USI_Ponder value true";
        }
        else if (line.find("setoption name ReplyDelay value ") == 0) delay = std::stoi(line.substr(32));
        else if (line.find("go ponder") == 0) { join(); pondering = true; }
        else if (line == "stop") {
            join();
            pondering = false;
            std::cout << "bestmove resign" << std::endl; // 予測局面の投了は破棄する
        } else if (line == "ponderhit" || line.find("go ") == 0) {
            if (line == "ponderhit" && !pondering) { join(); return 2; }
            if (pondering && line != "ponderhit") continue;
            join();
            pondering = false;
            const int turn = ++searches;
            const bool predict = ponderEnabled && mode != "no-prediction";
            worker = std::thread([delay, turn, predict] {
                std::this_thread::sleep_for(std::chrono::milliseconds(delay));
                std::cout << (turn == 1 ? "bestmove 8c8d" : "bestmove 3c3d");
                if (predict) std::cout << (turn == 1 ? " ponder 2g2f" : " ponder 2f2e");
                std::cout << std::endl;
            });
        } else if (line == "quit") break;
    }
    join();
}
