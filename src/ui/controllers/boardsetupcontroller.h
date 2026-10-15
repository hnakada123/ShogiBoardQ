#ifndef BOARDSETUPCONTROLLER_H
#define BOARDSETUPCONTROLLER_H

/// @file boardsetupcontroller.h
/// @brief 盤面初期設定コントローラクラスの定義


#include <QObject>
#include <QPoint>
#include <QPointer>
#include <functional>

#include "playmode.h"
#include "shogigamecontroller.h"  // Turn

class ShogiView;
class BoardInteractionController;
class MatchCoordinator;
class PositionEditController;
class EvaluationGraphController;
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
    void setPositionEditController(PositionEditController* posEdit);

    // --------------------------------------------------------
    // 状態設定
    // --------------------------------------------------------
    PlayMode playMode() const;

    void setSfenRecord(QStringList* sfenRecord);
    void setGameMoves(QList<ShogiMove>* gameMoves);

    // --------------------------------------------------------
    // BoardInteractionControllerアクセス
    // --------------------------------------------------------
    BoardInteractionController* boardController() const;

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

    // --------------------------------------------------------
    // コールバック設定
    // --------------------------------------------------------
    /// 対局していないときの盤上の着手を棋譜の変化として扱うか
    using IsVariationInputCallback = std::function<bool()>;
    /// 盤上の着手を変化として記録する（合法手で記録できたら true）
    using RecordVariationMoveCallback = std::function<bool(QPoint& from, QPoint& to)>;

    void setVariationInputCallbacks(IsVariationInputCallback isVariationInput,
                                    RecordVariationMoveCallback recordVariationMove);
    /// EvE で着手のたびに評価値グラフを描き直す先
    void setEvaluationGraphController(EvaluationGraphController* controller) { m_evalGraph = controller; }

public slots:
    /**
     * @brief 着手要求を処理
     */
    void onMoveRequested(const QPoint& from, const QPoint& to);

    /**
     * @brief 着手確定時の処理
     */
    void onMoveCommitted(Turn mover, int ply);

signals:
    /**
     * @brief CSA通信対局での指し手要求
     * @param from 移動元
     * @param to 移動先
     * @param promote 成るかどうか
     */
    void csaMoveRequested(const QPoint& from, const QPoint& to, bool promote);

private:
    void disconnectBoardInteractionController(BoardInteractionController* controller);

    ShogiView* m_shogiView = nullptr;
    ShogiGameController* m_gameController = nullptr;
    QPointer<BoardInteractionController> m_boardController;
    QPointer<MatchCoordinator> m_match;                  ///< 非所有（MCW が所有）
    PositionEditController* m_posEdit = nullptr;
    QObject* m_wheelFilter = nullptr;

    QStringList* m_sfenHistory = nullptr;
    QList<ShogiMove>* m_gameMoves = nullptr;

    QString m_lastMove;

    // コールバック
    IsVariationInputCallback m_isVariationInput;
    RecordVariationMoveCallback m_recordVariationMove;
    EvaluationGraphController* m_evalGraph = nullptr;
};

#endif // BOARDSETUPCONTROLLER_H
