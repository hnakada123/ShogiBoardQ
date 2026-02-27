#ifndef MAINWINDOWSTATE_H
#define MAINWINDOWSTATE_H

/// @file mainwindowstate.h
/// @brief MainWindow のサブシステム単位の状態集約構造体

#include <QDockWidget>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVector>

#include "playmode.h"
#include "shogimove.h"

// --- 前方宣言（DataModels用） ---
class KifuRecordListModel;
class KifuAnalysisListModel;
class KifuBranchListModel;
class ShogiEngineThinkingModel;
class UsiCommLogModel;
class GameRecordModel;

// --- 前方宣言（BranchNavigation用） ---
class KifuBranchTree;
class KifuNavigationState;
class KifuNavigationController;
class KifuDisplayCoordinator;
class BranchTreeWidget;
class LiveGameSession;

// --- 前方宣言（KifuState用） ---
class KifuDisplay;

/// ドックウィジェット群
struct DockWidgets {
    QDockWidget* evalChart = nullptr;
    QDockWidget* recordPane = nullptr;
    QDockWidget* gameInfo = nullptr;
    QDockWidget* thinking = nullptr;
    QDockWidget* consideration = nullptr;
    QDockWidget* usiLog = nullptr;
    QDockWidget* csaLog = nullptr;
    QDockWidget* comment = nullptr;
    QDockWidget* branchTree = nullptr;
    QDockWidget* menuWindow = nullptr;
    QDockWidget* josekiWindow = nullptr;
    QDockWidget* analysisResults = nullptr;
};

/// 対局者状態
struct PlayerState {
    QString humanName1;
    QString humanName2;
    QString engineName1;
    QString engineName2;
    bool bottomIsP1 = true;
    bool lastP1Turn = true;
    qint64 lastP1Ms = 0;
    qint64 lastP2Ms = 0;
};

/// データモデル群
struct DataModels {
    KifuRecordListModel* kifuRecord = nullptr;
    KifuBranchListModel* kifuBranch = nullptr;
    ShogiEngineThinkingModel* thinking1 = nullptr;
    ShogiEngineThinkingModel* thinking2 = nullptr;
    ShogiEngineThinkingModel* consideration = nullptr;
    KifuAnalysisListModel* analysis = nullptr;
    UsiCommLogModel* commLog1 = nullptr;
    UsiCommLogModel* commLog2 = nullptr;
    GameRecordModel* gameRecord = nullptr;
};

/// 分岐ナビゲーション
struct BranchNavigation {
    KifuBranchTree* branchTree = nullptr;
    KifuNavigationState* navState = nullptr;
    KifuNavigationController* kifuNavController = nullptr;
    KifuDisplayCoordinator* displayCoordinator = nullptr;
    BranchTreeWidget* branchTreeWidget = nullptr;
    LiveGameSession* liveGameSession = nullptr;
};

/// 棋譜状態
struct KifuState {
    QStringList positionStrList;
    QStringList gameUsiMoves;
    QVector<QString> commentsByRow;
    int activePly = 0;
    int currentSelectedPly = 0;
    bool onMainRowGuard = false;
    QList<KifuDisplay*> moveRecords;
    QVector<ShogiMove> gameMoves;
    QString saveFileName;
};

/// ゲーム状態
struct GameState {
    QString startSfenStr = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
    QString currentSfenStr = QStringLiteral("startpos");
    QString resumeSfenStr;
    bool errorOccurred = false;
    int currentMoveIndex = 0;
    PlayMode playMode = PlayMode::NotStarted;
    bool skipBoardSyncForBranchNav = false;
};

#endif // MAINWINDOWSTATE_H
