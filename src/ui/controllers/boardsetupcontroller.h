#ifndef BOARDSETUPCONTROLLER_H
#define BOARDSETUPCONTROLLER_H

/// @file boardsetupcontroller.h
/// @brief 盤面初期設定コントローラクラスの定義
/// @todo remove コメントスタイルガイド適用済み


#include <QObject>
#include <QPoint>
#include <functional>

#include "mainwindow.h"  // PlayMode enum
#include "shogigamecontroller.h"  // ShogiGameController::Player

class ShogiView;
class BoardInteractionController;
class MatchCoordinator;
class TimeControlController;
class PositionEditController;
struct ShogiMove;

/**
 * @brief BoardSetupController - 盤面操作配線管理クラス
 *
 * MainWindowから盤面操作の配線・セットアップを分離したクラス。
 * 以下の責務を担当:
 * - BoardInteractionControllerの生成・配線
 * - 盤クリックシグナルの接続
 * - 着手要求の処理
 */
class BoardSetupController : public QObject
{
    Q_OBJECT

public:
    explicit BoardSetupController(QObject* parent = nullptr);
    ~BoardSetupController() override;

    // --------------------------------------------------------
    // 依存オブジェクトの設定
    // --------------------------------------------------------
    void setShogiView(ShogiView* view);
    void setGameController(ShogiGameController* gc);
    void setMatchCoordinator(MatchCoordinator* match);
    void setTimeController(TimeControlController* tc);
    void setPositionEditController(PositionEditController* posEdit);

    // --------------------------------------------------------
    // 状態設定
    // --------------------------------------------------------
    void setPlayMode(PlayMode mode);
    PlayMode playMode() const { return m_playMode; }

    void setSfenRecord(QStringList* sfenRecord);
    void setGameMoves(QVector<ShogiMove>* gameMoves);
    void setCurrentMoveIndex(int* currentMoveIndex);

    // --------------------------------------------------------
    // BoardInteractionControllerアクセス
    // --------------------------------------------------------
    BoardInteractionController* boardController() const { return m_boardController; }

    // --------------------------------------------------------
    // セットアップ・配線
    // --------------------------------------------------------

    /**
     * @brief BoardInteractionControllerを生成・配線
     */
    void setupBoardInteractionController();

    /**
     * @brief 盤クリックシグナルを接続
     */
    void connectBoardClicks();

    /**
     * @brief 着手要求シグナルを接続
     */
    void connectMoveRequested();

    // --------------------------------------------------------
    // コールバック設定
    // --------------------------------------------------------
    using EnsurePositionEditCallback = std::function<void()>;
    using EnsureTimeControllerCallback = std::function<void()>;
    using UpdateGameRecordCallback = std::function<void(const QString& moveText, const QString& elapsed)>;
    using RedrawEngine1GraphCallback = std::function<void(int ply)>;
    using RedrawEngine2GraphCallback = std::function<void(int ply)>;
    using RefreshBranchTreeCallback = std::function<void()>;

    void setEnsurePositionEditCallback(EnsurePositionEditCallback cb);
    void setEnsureTimeControllerCallback(EnsureTimeControllerCallback cb);
    void setUpdateGameRecordCallback(UpdateGameRecordCallback cb);
    void setRedrawEngine1GraphCallback(RedrawEngine1GraphCallback cb);
    void setRedrawEngine2GraphCallback(RedrawEngine2GraphCallback cb);
    void setRefreshBranchTreeCallback(RefreshBranchTreeCallback cb);

public Q_SLOTS:
    /**
     * @brief 着手要求を処理
     */
    void onMoveRequested(const QPoint& from, const QPoint& to);

    /**
     * @brief 着手確定時の処理
     */
    void onMoveCommitted(ShogiGameController::Player mover, int ply);

Q_SIGNALS:
    /**
     * @brief 着手が適用された
     */
    void moveApplied(const QPoint& from, const QPoint& to, bool success);

    /**
     * @brief CSA通信対局での指し手要求
     * @param from 移動元
     * @param to 移動先
     * @param promote 成るかどうか
     */
    void csaMoveRequested(const QPoint& from, const QPoint& to, bool promote);

private:
    ShogiView* m_shogiView = nullptr;
    ShogiGameController* m_gameController = nullptr;
    BoardInteractionController* m_boardController = nullptr;
    MatchCoordinator* m_match = nullptr;
    TimeControlController* m_timeController = nullptr;
    PositionEditController* m_posEdit = nullptr;
    QObject* m_wheelFilter = nullptr;

    PlayMode m_playMode = PlayMode::NotStarted;
    QStringList* m_sfenRecord = nullptr;
    QVector<ShogiMove>* m_gameMoves = nullptr;
    int* m_currentMoveIndex = nullptr;

    QString m_lastMove;

    // コールバック
    EnsurePositionEditCallback m_ensurePositionEdit;
    EnsureTimeControllerCallback m_ensureTimeController;
    UpdateGameRecordCallback m_updateGameRecord;
    RedrawEngine1GraphCallback m_redrawEngine1Graph;
    RedrawEngine2GraphCallback m_redrawEngine2Graph;
    RefreshBranchTreeCallback m_refreshBranchTree;
};

#endif // BOARDSETUPCONTROLLER_H
