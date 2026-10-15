/// @file bench_movevalidator.cpp
/// @brief EngineMoveValidator パフォーマンスベンチマーク (毎回読み込む省略形 と 読み込んだ Context の使い回し)

#include <QElapsedTimer>
#include <QList>
#include <QMap>
#include <cstdio>

#include "enginemovevalidator.h"
#include "shogiboard.h"
#include "shogimove.h"

namespace {

struct BenchResult {
    double engineCompatMs = 0.0;
    double engineContextMs = 0.0;
    int iterations = 0;
    const char* label = "";
};

void printResult(const BenchResult& r)
{
    std::printf("  %-40s  %8d iter\n", r.label, r.iterations);
    std::printf("    EngineMV (compat wrapper):  %8.2f ms\n", r.engineCompatMs);
    std::printf("    EngineMV (Context API):     %8.2f ms", r.engineContextMs);
    if (r.engineCompatMs > 0) {
        std::printf("    [%.1fx vs compat]", r.engineCompatMs / r.engineContextMs);
    }
    std::printf("\n\n");
}

// ============================================================
// Benchmark 1: generateLegalMoves (平手)
// ============================================================
BenchResult benchGenerateLegalMoves(const QString& sfen, int iterations,
                                     Turn eTurn)
{
    ShogiBoard board;
    board.setSfen(sfen);
    const auto& bd = board.boardData();
    const auto& ps = board.pieceStand();

    BenchResult r;
    r.iterations = iterations;
    r.label = "generateLegalMoves";

    // --- 毎回読み込む ---
    {
        QElapsedTimer timer;
        timer.start();
        volatile int count = 0;
        for (int i = 0; i < iterations; ++i) {
            count = EngineMoveValidator::legalMoveCount(EngineMoveValidator::load(bd, ps, eTurn));
        }
        r.engineCompatMs = static_cast<double>(timer.nsecsElapsed()) / 1e6;
        (void)count;
    }

    // --- 読み込んだ Context を使い回す ---
    {
        EngineMoveValidator::Context ctx;
        ctx = EngineMoveValidator::load(bd, ps, eTurn);

        QElapsedTimer timer;
        timer.start();
        volatile int count = 0;
        for (int i = 0; i < iterations; ++i) {
            count = EngineMoveValidator::legalMoveCount(ctx);
        }
        r.engineContextMs = static_cast<double>(timer.nsecsElapsed()) / 1e6;
        (void)count;
    }

    return r;
}

// ============================================================
// Benchmark 2: checkIfKingInCheck
// ============================================================
BenchResult benchCheckIfKingInCheck(const QString& sfen, int iterations,
                                     Turn eTurn)
{
    ShogiBoard board;
    board.setSfen(sfen);
    const auto& bd = board.boardData();

    BenchResult r;
    r.iterations = iterations;
    r.label = "checkIfKingInCheck";

    // --- 毎回読み込む ---
    {
        QElapsedTimer timer;
        timer.start();
        volatile int count = 0;
        for (int i = 0; i < iterations; ++i) {
            count = EngineMoveValidator::checkCount(EngineMoveValidator::load(bd, QMap<Piece, int>(), eTurn));
        }
        r.engineCompatMs = static_cast<double>(timer.nsecsElapsed()) / 1e6;
        (void)count;
    }

    // --- 読み込んだ Context を使い回す ---
    {
        EngineMoveValidator::Context ctx;
        QMap<Piece, int> emptyStand;
        ctx = EngineMoveValidator::load(bd, emptyStand, eTurn);

        QElapsedTimer timer;
        timer.start();
        volatile int count = 0;
        for (int i = 0; i < iterations; ++i) {
            count = EngineMoveValidator::checkCount(ctx);
        }
        r.engineContextMs = static_cast<double>(timer.nsecsElapsed()) / 1e6;
        (void)count;
    }

    return r;
}

// ============================================================
// Benchmark 3: isLegalMove (1手判定を繰り返す)
// ============================================================
BenchResult benchIsLegalMove(const QString& sfen, int iterations,
                              Turn eTurn,
                              ShogiMove move, const char* label)
{
    ShogiBoard board;
    board.setSfen(sfen);
    const auto& bd = board.boardData();
    const auto& ps = board.pieceStand();

    BenchResult r;
    r.iterations = iterations;
    r.label = label;

    // --- 毎回読み込む ---
    {
        QElapsedTimer timer;
        timer.start();
        for (int i = 0; i < iterations; ++i) {
            auto st = EngineMoveValidator::legalStatus(EngineMoveValidator::load(bd, ps, eTurn), move.move);
            (void)st;
        }
        r.engineCompatMs = static_cast<double>(timer.nsecsElapsed()) / 1e6;
    }

    // --- 読み込んだ Context を使い回す ---
    {
        EngineMoveValidator::Context ctx;
        ctx = EngineMoveValidator::load(bd, ps, eTurn);

        QElapsedTimer timer;
        timer.start();
        for (int i = 0; i < iterations; ++i) {
            auto st = EngineMoveValidator::legalStatus(ctx, move.move);
            (void)st;
        }
        r.engineContextMs = static_cast<double>(timer.nsecsElapsed()) / 1e6;
    }

    return r;
}

// ============================================================
// Benchmark 4: Context sync+query vs compat (局面ごと)
// ============================================================
BenchResult benchSyncAndQuery(const QString& sfen, int iterations,
                               Turn eTurn)
{
    ShogiBoard board;
    board.setSfen(sfen);
    const auto& bd = board.boardData();
    const auto& ps = board.pieceStand();

    BenchResult r;
    r.iterations = iterations;
    r.label = "sync + generateLegalMoves";

    // --- 毎回読み込む ---
    {
        QElapsedTimer timer;
        timer.start();
        volatile int count = 0;
        for (int i = 0; i < iterations; ++i) {
            count = EngineMoveValidator::legalMoveCount(EngineMoveValidator::load(bd, ps, eTurn));
        }
        r.engineCompatMs = static_cast<double>(timer.nsecsElapsed()) / 1e6;
        (void)count;
    }

    // --- EngineMoveValidator (Context: sync + query each iteration) ---
    {
        QElapsedTimer timer;
        timer.start();
        volatile int count = 0;
        for (int i = 0; i < iterations; ++i) {
            EngineMoveValidator::Context ctx;
            ctx = EngineMoveValidator::load(bd, ps, eTurn);
            count = EngineMoveValidator::legalMoveCount(ctx);
        }
        r.engineContextMs = static_cast<double>(timer.nsecsElapsed()) / 1e6;
        (void)count;
    }

    return r;
}

// ============================================================
// Benchmark 5: 持ち駒ありの中盤局面
// ============================================================
BenchResult benchMidgame(int iterations)
{
    // 中盤: 持ち駒あり
    QString sfen = QStringLiteral(
        "ln1g1g1nl/1ks2r3/1ppppsbpp/p4pp2/9/2P1P4/PPBP1PPPP/2G1S2R1/LN2KG1NL b Pp 1");
    ShogiBoard board;
    board.setSfen(sfen);
    const auto& bd = board.boardData();
    const auto& ps = board.pieceStand();

    BenchResult r;
    r.iterations = iterations;
    r.label = "midgame (with hand pieces)";

    // --- 毎回読み込む ---
    {
        QElapsedTimer timer;
        timer.start();
        volatile int count = 0;
        for (int i = 0; i < iterations; ++i) {
            count = EngineMoveValidator::legalMoveCount(EngineMoveValidator::load(bd, ps, Turn::Black));
        }
        r.engineCompatMs = static_cast<double>(timer.nsecsElapsed()) / 1e6;
        (void)count;
    }

    // --- 読み込んだ Context を使い回す ---
    {
        EngineMoveValidator::Context ctx;
        ctx = EngineMoveValidator::load(bd, ps, Turn::Black);

        QElapsedTimer timer;
        timer.start();
        volatile int count = 0;
        for (int i = 0; i < iterations; ++i) {
            count = EngineMoveValidator::legalMoveCount(ctx);
        }
        r.engineContextMs = static_cast<double>(timer.nsecsElapsed()) / 1e6;
        (void)count;
    }

    return r;
}

} // namespace

int main()
{
    QString hirate = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
    QString checkSfen = QStringLiteral("lnsgk1snl/1r4gb1/ppppppppp/9/9/4r4/PPPP1PPPP/1B5R1/LNSGKGSNL b - 1");

    constexpr int N = 10000;

    std::printf("=== EngineMoveValidator Benchmark (compat vs Context) ===\n");
    std::printf("  Context: %zu bytes (position: %zu bytes)\n",
                sizeof(EngineMoveValidator::Context), sizeof(fmv::EnginePosition));
    std::printf("  Iterations: %d\n\n", N);

    printResult(benchGenerateLegalMoves(hirate, N, Turn::Black));

    printResult(benchCheckIfKingInCheck(hirate, N * 10, Turn::Black));

    printResult(benchCheckIfKingInCheck(checkSfen, N * 10, Turn::Black));

    printResult(benchIsLegalMove(hirate, N * 10, Turn::Black,
        ShogiMove(UsiMove::fromString(u"7g7f"), Piece::BlackPawn),
        "isLegalMove (single move)"));

    printResult(benchSyncAndQuery(hirate, N, Turn::Black));

    printResult(benchMidgame(N));

    // 成り・不成の両方を調べるGUI経路。各ケースとも同じ指し手を繰り返す。
    printResult(benchIsLegalMove(
        QStringLiteral("lnsgkgsnl/1r5b1/pppp1pppp/4P4/9/9/PPPP1PPPP/1B5R1/LNSGKGSNL b - 1"),
        N * 10, Turn::Black,
        ShogiMove(UsiMove::fromString(u"5d5c"), Piece::BlackPawn),
        "isLegalMove (optional promotion)"));
    printResult(benchIsLegalMove(
        QStringLiteral("1nsgkgsnl/Pr5b1/1pppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"),
        N * 10, Turn::Black,
        ShogiMove(UsiMove::fromString(u"9b9a"), Piece::BlackPawn),
        "isLegalMove (mandatory promotion)"));
    printResult(benchIsLegalMove(
        QStringLiteral("k8/9/4s4/4S4/9/9/9/9/4K4 b - 1"),
        N * 10, Turn::Black,
        ShogiMove(UsiMove::fromString(u"5d5c"), Piece::BlackSilver, Piece::WhiteSilver),
        "isLegalMove (promotion with capture)"));
    printResult(benchIsLegalMove(
        QStringLiteral("k3r4/9/9/4S4/9/9/9/9/4K4 b - 1"),
        N * 10, Turn::Black,
        ShogiMove(UsiMove::fromString(u"5d4c"), Piece::BlackSilver),
        "isLegalMove (pinned promotion)"));

    return 0;
}
