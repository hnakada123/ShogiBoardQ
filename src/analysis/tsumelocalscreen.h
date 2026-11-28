#ifndef TSUMELOCALSCREEN_H
#define TSUMELOCALSCREEN_H

/// @file tsumelocalscreen.h
/// @brief 内蔵の詰み探索（Hayanagi）による、詰将棋の候補局面の事前ふるい分け

#include "mateoracle.h"

#include <QString>
#include <QStringList>

#include <position.h>

#include <random>

/// 外部の詰将棋エンジンへ問い合わせる前に、内蔵の詰み探索で数ミリ秒だけ調べる検査。
/// 明らかに使えない候補と明らかな余詰を落とし、エンジンへの問い合わせを減らす。
namespace TsumeLocalScreen {

/// 候補局面のふるい分けの結果
enum class Outcome {
    Reject,         ///< 使わない
    Solved,         ///< 内蔵探索で詰み手順まで求めた
    DeferToEngine,  ///< 外部エンジンに判定させる
};

/// 主手順の上で見つけた余詰
struct MainLineCook {
    int ply = -1;       ///< 余詰のある攻方の手番（0 が初手）。無ければ -1
    QString mainMove;   ///< その手番の主手順の手
    QString cookMove;   ///< 余詰の手（無ければ空）
};

/// 外部エンジン（KomoringHeights）へ IPC 送信する前に、内蔵 Hayanagi で超高速プレスクリーニングを行う。
/// - 確定不詰（NoMate）や高層展開からの浅すぎる早詰・泥沼タイムアウトを数ミリ秒で棄却し、IPC の無駄撃ちを防ぐ。
/// - 19手以下では目標手数までの詰み・PV を直接抽出し、本命候補だけを KomoringHeights の厳密証明へ回す。
Outcome screenCandidate(const QString& sfen, int targetMoves, int minDesiredPlies,
                        int maxPliesReached, bool fromRepair, int stepTimeoutMs,
                        std::mt19937_64& rng, MateAnswer* solvedOut);

/// 63手以下のとき、内蔵 Hayanagi で超短時間に別初手余詰を検出する（検出できたらその初手USIを返す）
QString quickCookCheck(const QString& sfen, const QString& mainMove, int plies, int perMoveBudgetMs = 4);

/// 61手以下のとき、主手順の各攻方手番（1, 3, ..., N-2手目）について内蔵 Hayanagi で
/// 明白な余詰を数ミリ秒で事前検出し、厳密証明の無駄撃ちを防ぐ。
MainLineCook quickMainLineCookCheck(const QString& sfen, const QStringList& pv, int perMoveBudgetMs = 2);

/// 攻方の手番の局面に1手詰があるか
bool hasImmediateMate(const shogi::Position& position);

/// 初手余詰の検査で応手を調べる順（小さいほど先。逃れになりやすい応手から調べる）
int defensePriority(const shogi::Position& beforeDefense, const shogi::Move& defense);

} // namespace TsumeLocalScreen

#endif // TSUMELOCALSCREEN_H
