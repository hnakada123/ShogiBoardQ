#include <QCoreApplication>
#include <QFile>
#include <QTextStream>
#include <QThread>

#include <iostream>
#include <string>

namespace {
constexpr auto kPidEnvName = "SBQ_TEST_ENGINE_PID_FILE";
constexpr auto kModeEnvName = "SBQ_TEST_ENGINE_MODE";
constexpr auto kSlowUsiMode = "slow-usi";
constexpr auto kSlowQuitMode = "slow-quit";

void writePidFile()
{
    const QString pidFilePath = QString::fromLocal8Bit(qgetenv(kPidEnvName));
    if (pidFilePath.isEmpty()) {
        return;
    }

    QFile pidFile(pidFilePath);
    if (!pidFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return;
    }

    QTextStream stream(&pidFile);
    stream << QCoreApplication::applicationPid() << Qt::endl;
}
} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    writePidFile();

    const QByteArray mode = qgetenv(kModeEnvName);

    std::string line;
    while (std::getline(std::cin, line)) {
        if (mode == QByteArray(kSlowUsiMode)) {
            if (line == "usi") {
                QThread::sleep(10);
            }
            continue;
        }

        if (mode == QByteArray(kSlowQuitMode)) {
            if (line == "usi") {
                std::cout << "id name SlowQuitEngine" << std::endl;
                std::cout << "id author Test" << std::endl;
                std::cout << "usiok" << std::endl;
            } else if (line == "quit") {
                QThread::sleep(10);
                return 0;
            }
            continue;
        }
    }

    return 0;
}
