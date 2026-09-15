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
    const auto& ps = board.pieceStand();

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
                              EngineMoveValidator::Turn eTurn,
                              ShogiMove move, const char* label)
{
    ShogiBoard board;
    board.setSfen(sfen);
    const auto& bd = board.boardData();
    const auto& ps = board.pieceStand();

    BenchResult r;
    r.iterations = iterations;
    r.label = label;

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
    const auto& ps = board.pieceStand();

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
    const auto& ps = board.pieceStand();

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
    std::printf("  Context: %zu bytes (position: %zu bytes)\n",
                sizeof(EngineMoveValidator::Context), sizeof(fmv::EnginePosition));
    std::printf("  Iterations: %d\n\n", N);

    printResult(benchGenerateLegalMoves(hirate, N, EngineMoveValidator::BLACK));

    printResult(benchCheckIfKingInCheck(hirate, N * 10, EngineMoveValidator::BLACK));

    printResult(benchCheckIfKingInCheck(checkSfen, N * 10, EngineMoveValidator::BLACK));

    printResult(benchIsLegalMove(hirate, N * 10, EngineMoveValidator::BLACK,
        ShogiMove(QPoint(6, 6), QPoint(6, 5), Piece::BlackPawn, Piece::None, false),
        "isLegalMove (single move)"));

    printResult(benchSyncAndQuery(hirate, N, EngineMoveValidator::BLACK));

    printResult(benchMidgame(N));

    // 成り・不成の両方を調べるGUI経路。各ケースとも同じ指し手を繰り返す。
    printResult(benchIsLegalMove(
        QStringLiteral("lnsgkgsnl/1r5b1/pppp1pppp/4P4/9/9/PPPP1PPPP/1B5R1/LNSGKGSNL b - 1"),
        N * 10, EngineMoveValidator::BLACK,
        ShogiMove(QPoint(4, 3), QPoint(4, 2), Piece::BlackPawn, Piece::None, false),
        "isLegalMove (optional promotion)"));
    printResult(benchIsLegalMove(
        QStringLiteral("1nsgkgsnl/Pr5b1/1pppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"),
        N * 10, EngineMoveValidator::BLACK,
        ShogiMove(QPoint(8, 1), QPoint(8, 0), Piece::BlackPawn, Piece::None, false),
        "isLegalMove (mandatory promotion)"));
    printResult(benchIsLegalMove(
        QStringLiteral("k8/9/4s4/4S4/9/9/9/9/4K4 b - 1"),
        N * 10, EngineMoveValidator::BLACK,
        ShogiMove(QPoint(4, 3), QPoint(4, 2), Piece::BlackSilver, Piece::WhiteSilver, false),
        "isLegalMove (promotion with capture)"));
    printResult(benchIsLegalMove(
        QStringLiteral("k3r4/9/9/4S4/9/9/9/9/4K4 b - 1"),
        N * 10, EngineMoveValidator::BLACK,
        ShogiMove(QPoint(4, 3), QPoint(3, 2), Piece::BlackSilver, Piece::None, false),
        "isLegalMove (pinned promotion)"));

    return 0;
}
