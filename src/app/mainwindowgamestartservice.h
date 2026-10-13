#ifndef MAINWINDOWGAMESTARTSERVICE_H
#define MAINWINDOWGAMESTARTSERVICE_H

/// @file mainwindowgamestartservice.h
/// @brief MainWindow の対局開始前処理を分離したサービス

#include "gamestartcoordinator.h"
#include "recordline.h"

#include <QString>
#include <QStringList>

class ShogiGameController;
class ShogiClock;
class KifuRecordListModel;
class KifuLoadCoordinator;

/**
 * @brief MainWindow::initializeGame 周辺の準備処理を担当するサービス
 *
 * 責務:
 * - 分岐ライン選択時の SFEN 履歴再構築
 * - 対局開始時に使用する currentSfen の解決
 * - GameStartCoordinator::Ctx の構築
 */
class MainWindowGameStartService
{
public:
    struct PrepareDeps {
        RecordLine line;                         ///< 表示中の手順（分岐ツリー）。currentPly が開始する手数
        QStringList* sfenRecord = nullptr;       ///< 対局の局面列（ここで line から作り直す）
        QList<ShogiMove>* gameMoves = nullptr;   ///< 対局の指し手列（局面列とそろえて作り直す）
        QString* currentSfenStr = nullptr;
    };

    struct ContextDeps {
        ShogiGameController* gc = nullptr;
        ShogiClock* clock = nullptr;
        QStringList* sfenRecord = nullptr;
        KifuRecordListModel* kifuModel = nullptr;
        KifuLoadCoordinator* kifuLoadCoordinator = nullptr;
        QString* currentSfenStr = nullptr;
        QString* startSfenStr = nullptr;
        int selectedPly = 0;
        bool isReplayMode = false;
        bool bottomIsP1 = true;
    };

    /// initializeGame 実行前に SFEN 関連状態を準備する
    void prepareForInitializeGame(const PrepareDeps& deps) const;

    /// GameStartCoordinator::initializeGame へ渡すコンテキストを構築する
    GameStartCoordinator::Ctx buildContext(const ContextDeps& deps) const;

private:
    void rebuildMatchHistoryFromLine(const PrepareDeps& deps) const;
    QString resolveCurrentSfenForGameStart(const PrepareDeps& deps) const;
};

#endif // MAINWINDOWGAMESTARTSERVICE_H
