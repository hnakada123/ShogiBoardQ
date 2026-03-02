/// @file bench_movevalidator.cpp
/// @brief EngineMoveValidator パフォーマンスベンチマーク (compat wrapper vs Context API)

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
                                     EngineMoveValidator::Turn eTurn)
{
    ShogiBoard board;
    board.setSfen(sfen);
    const auto& bd = board.boardData();
    const auto& ps = board.getPieceStand();

    BenchResult r;
    r.iterations = iterations;
    r.label = "generateLegalMoves";

    // --- EngineMoveValidator (compat) ---
    {
        EngineMoveValidator emv;
        QElapsedTimer timer;
        timer.start();
        volatile int count = 0;
        for (int i = 0; i < iterations; ++i) {
            count = emv.generateLegalMoves(eTurn, bd, ps);
        }
        r.engineCompatMs = static_cast<double>(timer.nsecsElapsed()) / 1e6;
        (void)count;
    }

    // --- EngineMoveValidator (Context) ---
    {
        EngineMoveValidator emv;
        EngineMoveValidator::Context ctx;
        (void)emv.syncContext(ctx, eTurn, bd, ps);

        QElapsedTimer timer;
        timer.start();
        volatile int count = 0;
        for (int i = 0; i < iterations; ++i) {
            count = emv.generateLegalMoves(ctx);
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
                                     EngineMoveValidator::Turn eTurn)
{
    ShogiBoard board;
    board.setSfen(sfen);
    const auto& bd = board.boardData();

    BenchResult r;
    r.iterations = iterations;
    r.label = "checkIfKingInCheck";

    // --- EngineMoveValidator (compat) ---
    {
        EngineMoveValidator emv;
        QElapsedTimer timer;
        timer.start();
        volatile int count = 0;
        for (int i = 0; i < iterations; ++i) {
            count = emv.checkIfKingInCheck(eTurn, bd);
        }
        r.engineCompatMs = static_cast<double>(timer.nsecsElapsed()) / 1e6;
        (void)count;
    }

    // --- EngineMoveValidator (Context) ---
    {
        EngineMoveValidator emv;
        EngineMoveValidator::Context ctx;
        QMap<Piece, int> emptyStand;
        (void)emv.syncContext(ctx, eTurn, bd, emptyStand);

        QElapsedTimer timer;
        timer.start();
        volatile int count = 0;
        for (int i = 0; i < iterations; ++i) {
            count = emv.checkIfKingInCheck(ctx);
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
                              EngineMoveValidator::Turn eTurn)
{
    ShogiBoard board;
    board.setSfen(sfen);
    const auto& bd = board.boardData();
    const auto& ps = board.getPieceStand();

    // 7六歩
    ShogiMove move(QPoint(6, 6), QPoint(6, 5), Piece::BlackPawn, Piece::None, false);

    BenchResult r;
    r.iterations = iterations;
    r.label = "isLegalMove (single move)";

    // --- EngineMoveValidator (compat) ---
    {
        EngineMoveValidator emv;
        QElapsedTimer timer;
        timer.start();
        for (int i = 0; i < iterations; ++i) {
            auto st = emv.isLegalMove(eTurn, bd, ps, move);
            (void)st;
        }
        r.engineCompatMs = static_cast<double>(timer.nsecsElapsed()) / 1e6;
    }

    // --- EngineMoveValidator (Context) ---
    {
        EngineMoveValidator emv;
        EngineMoveValidator::Context ctx;
        (void)emv.syncContext(ctx, eTurn, bd, ps);

        QElapsedTimer timer;
        timer.start();
        for (int i = 0; i < iterations; ++i) {
            auto st = emv.isLegalMove(ctx, move);
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
                               EngineMoveValidator::Turn eTurn)
{
    ShogiBoard board;
    board.setSfen(sfen);
    const auto& bd = board.boardData();
    const auto& ps = board.getPieceStand();

    BenchResult r;
    r.iterations = iterations;
    r.label = "sync + generateLegalMoves";

    // --- EngineMoveValidator (compat) ---
    {
        EngineMoveValidator emv;
        QElapsedTimer timer;
        timer.start();
        volatile int count = 0;
        for (int i = 0; i < iterations; ++i) {
            count = emv.generateLegalMoves(eTurn, bd, ps);
        }
        r.engineCompatMs = static_cast<double>(timer.nsecsElapsed()) / 1e6;
        (void)count;
    }

    // --- EngineMoveValidator (Context: sync + query each iteration) ---
    {
        EngineMoveValidator emv;
        QElapsedTimer timer;
        timer.start();
        volatile int count = 0;
        for (int i = 0; i < iterations; ++i) {
            EngineMoveValidator::Context ctx;
            (void)emv.syncContext(ctx, eTurn, bd, ps);
            count = emv.generateLegalMoves(ctx);
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
    const auto& ps = board.getPieceStand();

    BenchResult r;
    r.iterations = iterations;
    r.label = "midgame (with hand pieces)";

    // --- EngineMoveValidator (compat) ---
    {
        EngineMoveValidator emv;
        QElapsedTimer timer;
        timer.start();
        volatile int count = 0;
        for (int i = 0; i < iterations; ++i) {
            count = emv.generateLegalMoves(EngineMoveValidator::BLACK, bd, ps);
        }
        r.engineCompatMs = static_cast<double>(timer.nsecsElapsed()) / 1e6;
        (void)count;
    }

    // --- EngineMoveValidator (Context) ---
    {
        EngineMoveValidator emv;
        EngineMoveValidator::Context ctx;
        (void)emv.syncContext(ctx, EngineMoveValidator::BLACK, bd, ps);

        QElapsedTimer timer;
        timer.start();
        volatile int count = 0;
        for (int i = 0; i < iterations; ++i) {
            count = emv.generateLegalMoves(ctx);
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
    std::printf("  Iterations: %d\n\n", N);

    printResult(benchGenerateLegalMoves(hirate, N, EngineMoveValidator::BLACK));

    printResult(benchCheckIfKingInCheck(hirate, N * 10, EngineMoveValidator::BLACK));

    printResult(benchCheckIfKingInCheck(checkSfen, N * 10, EngineMoveValidator::BLACK));

    printResult(benchIsLegalMove(hirate, N * 10, EngineMoveValidator::BLACK));

    printResult(benchSyncAndQuery(hirate, N, EngineMoveValidator::BLACK));

    printResult(benchMidgame(N));

    return 0;
}
