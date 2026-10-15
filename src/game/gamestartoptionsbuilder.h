#ifndef GAMESTARTOPTIONSBUILDER_H
#define GAMESTARTOPTIONSBUILDER_H

/// @file gamestartoptionsbuilder.h
/// @brief 対局開始ダイアログの値の解釈（時間・対局者）と開始局面プリセット

#include <QString>

#include "gamestartcoordinator.h"
#include "startgamedatabridge.h"
#include "startoptions.h"


/**
 * @brief 対局開始パラメータの構築を担う static ユーティリティクラス
 *
 * GameStartCoordinator から以下の責務を分離:
 *  - ダイアログの値解釈（時間設定・対局者）
 *  - 開始局面プリセットテーブル
 */
class GameStartOptionsBuilder
{
public:
    using TimeSettings = GameStartCoordinator::TimeSettings;

    // --- 時間設定の構築 ---

    /// StartGameDialogData から時間設定を組み立てる（秒読みと加算が両方あれば秒読みを使う）
    static TimeSettings buildTimeControl(const StartGameDialogData& data);

    // --- 対局者 ---

    /// ダイアログで選んだ対局者（人間・エンジンと名前）。人間とエンジンの両方に印があれば人間とする
    static MatchPlayers playersFromDialog(const StartGameDialogData& data);

    // --- 開始局面プリセット ---

    /// 手合割インデックス（1=平手, 2=香落ち, …, 14=十枚落ち）から純SFENを返す
    static const QString& startingPositionSfen(int index);

    /// プリセット数
    static constexpr int kStartingPositionCount = 14;

    // --- SFEN 正規化 ---

    /// "startpos" や空文字列を平手SFENに正規化する
    static QString canonicalizeSfen(const QString& sfen);

    /// 対局開始ダイアログの設定から、対局の開始の設定を作る
    static MatchStartOptions buildStartOptions(const QString& startSfen, const StartGameDialogData& dlg);

    /// 「人を手前に表示する」で盤を反転するか（人間対エンジンで、人間の側が手前でないとき）
    static bool needsHumanAtBottomFlip(const StartGameDialogData& dlg, Turn bottomSide);

};

#endif // GAMESTARTOPTIONSBUILDER_H
