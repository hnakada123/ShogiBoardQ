// 対局通信の回帰テスト用。先読み中の二重goには応答しない。
#include <iostream>
#include <string>
#include <thread>
#include <chrono>

int main()
{
    bool pondering = false;
    int searches = 0;
    int delay = 0;
    std::thread worker;
    auto join = [&] { if (worker.joinable()) worker.join(); };
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line == "usi") std::cout << "usiok" << std::endl;
        else if (line == "isready") std::cout << "readyok" << std::endl;
        else if (line.find("setoption name ReplyDelay value ") == 0) delay = std::stoi(line.substr(32));
        else if (line.find("go ponder") == 0) { join(); pondering = true; }
        else if (line == "stop") {
            join();
            pondering = false;
            std::cout << "bestmove resign" << std::endl; // 予測局面の投了は破棄する
        } else if (line == "ponderhit" || line.find("go ") == 0) {
            if (pondering && line != "ponderhit") continue;
            join();
            pondering = false;
            const int turn = ++searches;
            worker = std::thread([delay, turn] {
                std::this_thread::sleep_for(std::chrono::milliseconds(delay));
                std::cout << (turn == 1 ? "bestmove 8c8d ponder 2g2f"
                                        : "bestmove 3c3d ponder 2f2e") << std::endl;
            });
        } else if (line == "quit") break;
    }
    join();
}
