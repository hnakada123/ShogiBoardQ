#ifndef CSAGAMEPROGRESS_H
#define CSAGAMEPROGRESS_H

/// @file csagameprogress.h
/// @brief CSA通信対局1局ぶんの進行（手数・持ち時間・指し手列）を表す値型

#include <QString>
#include <QStringList>
#include <optional>

#include "csaclient.h"
#include "recordentry.h"
#include "recordline.h"
#include "recordtime.h"
#include "usigotime.h"

/**
 * @brief CSA通信対局1局ぶんの進行を表す値型
 *
 * CsaGameCoordinator が1つだけ持ち、対局条件（Game_Summary）の受信で作り直す。
 * 持ち時間の計算・棋譜の消費時間・終局行の組み立ては、ここの関数が値として返す。
 * 盤面・時計・サーバーとのやり取りは持たない（GUI やソケットなしで試せる）。
 */
struct CsaGameProgress {
    CsaClient::GameSummary summary;  ///< 受信した対局条件
    bool isBlackSide = true;         ///< 自分が先手か

    /// この対局の手順（開始局面と指された手。再開局面の手を含み、終局の行は含めない）
    RecordLine record;

    int blackTotalMs = 0;            ///< 先手の消費時間の累計
    int whiteTotalMs = 0;            ///< 後手の消費時間の累計
    int blackRemainingMs = 0;        ///< 先手の残り時間（サーバーの持時間の数え方）
    int whiteRemainingMs = 0;        ///< 後手の残り時間
    int resignConsumedMs = 0;        ///< 自分の投了時に時計から測った消費時間

    /// 対局条件を受けて、前局の記録を消す
    static CsaGameProgress forSummary(const CsaClient::GameSummary& summary);

    /**
     * @brief 再開局面（仕様 3.2.3）の持ち時間を求める
     *
     * Game_Summary の指し手が既に完了した状態にする。各手の消費時間を引き、各手番の開始時に
     * 加算時間を足す。手番側には、これから始まる手番の加算時間を足す。
     */
    void startClockFromSummary();

    /**
     * @brief 1手の消費時間を記録する
     * @param blackMove 先手の手か
     * @param consumedMs サーバーが示した消費時間
     *
     * 指した側の累計と残り時間を更新し、次の手番側に加算時間を足す（CSA の加算は手番の開始時）。
     */
    void recordMoveTime(bool blackMove, int consumedMs);

    /// 指された手数（再開局面の手を含む）
    int moveCount() const { return record.moveCount(); }
    /// 直前の手の移動先（「同」の判定用。手が無ければ無効な升）
    BoardSquare previousTo() const { return record.destinationOf(record.moveCount()); }
    /// エンジンに探索させる今の局面（開始局面からの手順。読めなければ nullopt）
    std::optional<UsiPosition> usiPosition() const { return record.usiPosition(record.moveCount()); }

    int totalMs(bool black) const { return black ? blackTotalMs : whiteTotalMs; }
    int remainingMs(bool black) const { return black ? blackRemainingMs : whiteRemainingMs; }

    /// 再開局面の index 手目の消費時間と累計の表記（棋譜欄用）
    RecordTime initialMoveTime(int index) const;

    /**
     * @brief 自分の手番でエンジンに渡す時間の条件
     * @param marginMs 通信の遅延に備えて差し引く余裕
     *
     * CSA の残り時間は手番の始めに加算するので、手番側は今の手番の加算を含む。USI の
     * btime/wtime の意味（その手番の加算を含まない）にそろえる。遅延時間は今の手にだけ足す。
     */
    UsiGoTimeInput engineTiming(int marginMs) const;

    /**
     * @brief 着手の棋譜行を組み立てる（盤面の値は呼び出し側が埋める）
     * @param blackMove 先手の手か
     * @param displayText 表示用の指し手
     * @param consumedMs 消費時間（recordMoveTime の後に呼ぶ）
     */
    RecordEntry moveRecord(bool blackMove, const QString& displayText, int consumedMs) const;

    /**
     * @brief 終局行を組み立てる
     * @param result 自分から見た勝敗
     * @param cause 終局原因
     * @param blackToMove 終局した局面で先手の手番か（連続王手の千日手の手番側）
     * @param consumedMs 終局行の消費時間
     *
     * 終局原因を TerminalType に対応させ、終局行を行った側（RecordEntry::endingSide。ローカルの対局と同じ規則）の
     * 印と消費時間を付ける。
     */
    RecordEntry endRecord(CsaClient::GameResult result, CsaClient::GameEndCause cause,
                          bool blackToMove, int consumedMs) const;
};

#endif // CSAGAMEPROGRESS_H
