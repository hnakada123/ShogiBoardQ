#ifndef CSAGAMESUMMARY_H
#define CSAGAMESUMMARY_H

/// @file csagamesummary.h
/// @brief CSA の対局条件（Game_Summary）の値

#include <QList>
#include <QString>
#include <QStringList>

#include "shogitypes.h"
#include "timecontrol.h"

#include <optional>

/**
 * @brief CSA サーバーが送る対局条件（BEGIN Game_Summary 〜 END Game_Summary）
 *
 * 読むのは CsaGameSummaryReader。値は受け取ったままで、ミリ秒への換算と持ち時間の決まりへの変換はここで行う。
 */
struct CsaGameSummary {
    /// 1つの時間の値（持時間・秒読み・加算・1手の消費時間）の上限（ミリ秒。7日）。
    /// サーバーが送る大きな値で、持ち時間の計算を桁あふれさせない（セキュリティ監査 S13）
    static constexpr int kMaxTimeMs = 7 * 24 * 60 * 60 * 1000;

    /**
     * @brief 片方の対局者の持時間（BEGIN Time / Time+ / Time- の内容）
     *
     * 値は Time_Unit の単位時間で持ち、*Ms() でミリ秒に換算する。
     */
    struct Time {
        QString timeUnit = QStringLiteral("1sec"); ///< 時間単位（1sec/1min/1msec）
        int totalTime = 0;          ///< 持時間の初期値
        int byoyomi = 0;            ///< 秒読み
        int leastTimePerMove = 0;   ///< 最少消費時間
        int increment = 0;          ///< 加算時間
        int delay = 0;              ///< 遅延時間
        bool timeRoundup = false;   ///< 単位時間未満を切り上げるか
        bool specified = false;     ///< Time 階層が記述されたか

        int unitMs() const;                 ///< 時間単位をミリ秒に換算
        /// 単位時間の数をミリ秒にする（0〜kMaxTimeMs。サーバーが送る大きな値で桁あふれさせない）
        int toMs(qint64 units) const;
        int totalTimeMs() const { return toMs(totalTime); }
        int byoyomiMs() const { return toMs(byoyomi); }
        int incrementMs() const { return toMs(increment); }
        int delayMs() const { return toMs(delay); }
        int leastTimePerMoveMs() const { return toMs(leastTimePerMove); }
        /// 時間無制限か（Time 階層の省略、または全時間要素の省略）
        bool isUnlimited() const
        {
            return !specified || (totalTime == 0 && byoyomi == 0 && increment == 0 && delay == 0);
        }
    };

    QString protocolVersion;    ///< プロトコルバージョン
    QString protocolMode = QStringLiteral("Server"); ///< プロトコルモード（Server/Direct）
    QString format;             ///< フォーマット（Shogi 1.0）
    QString declaration;        ///< 宣言ルール（Jishogi 1.1）
    QString gameId;             ///< 対局ID
    QString blackName;          ///< 先手名
    QString whiteName;          ///< 後手名
    std::optional<Turn> myTurn; ///< こちら側の手番（Your_Turn。届かなければ無し）
    Turn toMove = Turn::Black;  ///< 最初の手番（To_Move）
    bool rematchOnDraw = false; ///< 引き分け時再対局するか
    int maxMoves = 0;           ///< 最大手数（0=無制限）

    // 持時間情報。共通の Time は両方に入れ、Time+/Time- は該当側だけを上書きする
    Time timeBlack;             ///< 先手の持時間情報
    Time timeWhite;             ///< 後手の持時間情報
    bool hasIndividualTime = false; ///< Time+/Time- の個別設定があるか

    // 局面情報（CSA形式）
    QStringList positionLines;  ///< 初期局面（P1-P9, P+, P-, +/-）
    QStringList moves;          ///< 初期局面からの指し手リスト（再開局面）
    QList<int> moveConsumedTimes; ///< moves と同じ長さ。各手の消費時間（単位時間、,T 無しは 0）

    /// こちらが先手（下手）か
    bool isBlackSide() const { return myTurn == Turn::Black; }
    /// こちらが最初に指すか
    bool movesFirst() const { return myTurn == toMove; }
    const Time& timeFor(bool black) const { return black ? timeBlack : timeWhite; }
    Time& timeFor(bool black) { return black ? timeBlack : timeWhite; }
    int timeUnitMs(bool black) const { return timeFor(black).unitMs(); }
    /// black の側の単位時間の数をミリ秒にする（Time::toMs）
    int unitsToMs(bool black, qint64 units) const { return timeFor(black).toMs(units); }
    int totalTimeMs(bool black) const { return timeFor(black).totalTimeMs(); }
    int byoyomiMs(bool black) const { return timeFor(black).byoyomiMs(); }
    int incrementMs(bool black) const { return timeFor(black).incrementMs(); }
    int delayMs(bool black) const { return timeFor(black).delayMs(); }
    /// 双方とも時間無制限か
    bool isTimeUnlimited() const { return timeBlack.isUnlimited() && timeWhite.isUnlimited(); }
    /// 持ち時間の決まり（ミリ秒）。時間無制限なら値はすべて 0
    TimeControl timeControl() const;
    /// Declaration で入玉宣言ルール（Jishogi）が採用されているか
    bool allowsJishogiDeclaration() const { return declaration.startsWith(QStringLiteral("Jishogi")); }
    /// 再開局面の index 手目の消費時間（ミリ秒）。指した側の時間単位で換算する
    int positionMoveConsumedMs(int index) const;

    /// 指し手・意思表示の行（"+7776FU,T12"・"%TORYO,T4"）の消費時間（単位時間の数。無い・読めなければ 0）
    static int consumedUnits(const QString& line);
};

#endif // CSAGAMESUMMARY_H
