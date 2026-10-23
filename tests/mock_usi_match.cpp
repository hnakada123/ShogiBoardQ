// 対局通信の回帰テスト用。先読み中の二重goには応答しない。
#include <atomic>
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <csignal>

int main()
{
    if (std::getenv("SBQ_MATCH_IGNORE_TERMINATE")) std::signal(SIGTERM, SIG_IGN);
    bool pondering = false;
    bool ponderEnabled = true; // Gikou同様、未報告でも既定値はtrue
    const char* modeEnv = std::getenv("SBQ_MATCH_PONDER_MODE");
    const std::string mode = modeEnv ? modeEnv : "hidden";
    int searches = 0;
    // 指し手を返すまでの時間（setoption name ReplyDelay でも変えられる）
    const char* replyDelayEnv = std::getenv("SBQ_MATCH_REPLY_DELAY_MS");
    int delay = replyDelayEnv ? std::atoi(replyDelayEnv) : 0;
    std::thread worker;
    std::atomic<bool> stopRequested{false};   // 探索中の stop（考えるのをやめて手を答える）
    auto join = [&] { if (worker.joinable()) worker.join(); };
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line == "usi") {
            if (const char* delayMs = std::getenv("SBQ_MATCH_INIT_DELAY_MS"))
                std::this_thread::sleep_for(std::chrono::milliseconds(std::atoi(delayMs)));
            if (std::getenv("SBQ_MATCH_NO_USIOK")) continue;
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
        // 詰み探索に答えないエンジン（stop にも答えない）
        else if (std::getenv("SBQ_MATCH_IGNORE_MATE") && (line.find("go mate") == 0 || line == "stop")) continue;
        else if (line.find("go ponder") == 0) { join(); pondering = true; }
        else if (line == "stop") {
            if (const char* delayMs = std::getenv("SBQ_MATCH_STOP_DELAY_MS"))
                std::this_thread::sleep_for(std::chrono::milliseconds(std::atoi(delayMs)));
            if (pondering) {
                pondering = false;
                std::cout << "bestmove resign" << std::endl; // 予測局面の投了は破棄する
            } else if (worker.joinable()) {
                // 探索中なら考えるのをやめ、その手を答える（bestmove は1回だけ）
                stopRequested = true;
                join();
            }
        } else if (line == "ponderhit" || line.find("go ") == 0) {
            if (std::getenv("SBQ_MATCH_EXIT_ON_GO")) { join(); return 3; }
            if (line == "ponderhit" && !pondering) { join(); return 2; }
            if (pondering && line != "ponderhit") continue;
            join();
            pondering = false;
            const int turn = ++searches;
            const bool predict = ponderEnabled && mode != "no-prediction";
            stopRequested = false;
            worker = std::thread([delay, turn, predict, &stopRequested] {
                for (int waited = 0; waited < delay && !stopRequested; waited += 5)
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                std::cout << (turn == 1 ? "bestmove 8c8d" : "bestmove 3c3d");
                if (predict) std::cout << (turn == 1 ? " ponder 2g2f" : " ponder 2f2e");
                std::cout << std::endl;
            });
        } else if (line == "quit") break;
    }
    join();
}
