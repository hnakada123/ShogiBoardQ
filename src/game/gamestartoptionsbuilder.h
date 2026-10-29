#ifndef GAMESTARTOPTIONSBUILDER_H
#define GAMESTARTOPTIONSBUILDER_H

/// @file gamestartoptionsbuilder.h
/// @brief 対局開始ダイアログの値から、対局の開始の計画（局面・対局者・時間・連続対局）を作る

#include <QString>
#include <optional>

#include "matchtimesettings.h"
#include "startgamedialogdata.h"
#include "startoptions.h"

/// 対局を始める操作の入力
struct GameStartRequest {
    StartGameDialogData dialogData;  ///< 対局開始ダイアログの値
    RecordLine recordLine;           ///< 表示中の手順（現在局面から始めるときは currentPly までを使う）
    int startNodeId = -1;            ///< 表示中の手（分岐ツリーのノード）
    Turn bottomSide = Turn::Black;   ///< 盤の手前に表示している側
};

/// 対局の開始の計画（共有状態に触れずに作る。実行は GameSession）
struct GameStartPlan {
    MatchStartOptions options;          ///< 対局者・開始局面・手順
    MatchTimeSettings time;             ///< 時間の決まり
    GameStartPreparation preparation;   ///< 開始前に棋譜をどう整えるか
    bool flipBoard = false;             ///< 「人を手前に表示する」で盤を反転する
    int consecutiveGames = 1;           ///< 連続対局の局数
    bool switchTurnEachGame = false;    ///< 1局ごとに手番を入れ替える
};

/**
 * @brief 対局開始ダイアログの値の解釈と、開始の計画の組み立て（状態を持たない）
 */
class GameStartOptionsBuilder
{
public:
    /**
     * @brief ダイアログの値と表示中の手順から、開始の計画を作る
     * @param error 始められないときの理由（利用者に見せる文）
     */
    static std::optional<GameStartPlan> plan(const GameStartRequest& request, QString* error);

    /// 通常の対局に使える局面か（先手・後手の玉が盤上に1枚ずつで、PositionValidator の検査を通る）。
    /// 使えなければ理由を返す
    static bool validatePosition(const QString& sfen, QString* whyNot);

    /// StartGameDialogData から時間の決まりを組み立てる（秒読みと加算が両方あれば秒読みを使う）
    static MatchTimeSettings buildTimeSettings(const StartGameDialogData& data);
    /// ダイアログで選んだ対局者（人間・エンジンと名前）
    static MatchPlayers playersFromDialog(const StartGameDialogData& data);
    /// 対局開始ダイアログの設定から、対局の開始の設定を作る
    static MatchStartOptions buildStartOptions(const QString& startSfen, const StartGameDialogData& dlg);
    /// 「人を手前に表示する」で盤を反転するか（人間対エンジンで、人間の側が手前でないとき）
    static bool needsHumanAtBottomFlip(const StartGameDialogData& dlg, Turn bottomSide);

    /// "startpos" や空文字列を平手SFENに正規化する
    static QString canonicalizeSfen(const QString& sfen);
};

#endif // GAMESTARTOPTIONSBUILDER_H
