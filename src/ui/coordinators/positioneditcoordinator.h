#ifndef POSITIONEDITCOORDINATOR_H
#define POSITIONEDITCOORDINATOR_H

/// @file positioneditcoordinator.h
/// @brief 局面編集コーディネータクラスの定義


#include <QObject>
#include <QString>
#include <QStringList>
#include <functional>

class ShogiView;
class ShogiGameController;
class BoardInteractionController;
class PositionEditController;
class MatchCoordinator;
class ReplayController;
class QAction;

/**
 * @brief PositionEditCoordinator - 局面編集の調整クラス
 *
 * MainWindowから局面編集の開始/終了処理を分離したクラス。
 * 以下の責務を担当:
 * - 編集開始時のコンテキスト構築とUI設定
 * - 編集終了時のコンテキスト構築と後処理
 * - 編集用アクションの接続
 */
class PositionEditCoordinator : public QObject
{
    Q_OBJECT

public:
    explicit PositionEditCoordinator(QObject* parent = nullptr);
    ~PositionEditCoordinator() override;

    // --------------------------------------------------------
    // 依存オブジェクトの設定
    // --------------------------------------------------------
    void setShogiView(ShogiView* view);
    void setGameController(ShogiGameController* gc);
    void setBoardController(BoardInteractionController* bic);
    void setPositionEditController(PositionEditController* posEdit);
    void setMatchCoordinator(MatchCoordinator* match);
    void setReplayController(ReplayController* replay);
    void setSfenRecord(QStringList* sfenRecord);

    // --------------------------------------------------------
    // 状態参照の設定（ポインタ経由）
    // --------------------------------------------------------
    void setCurrentSelectedPly(const int* currentSelectedPly);
    void setActivePly(const int* activePly);
    void setStartSfenStr(QString* startSfenStr);
    void setCurrentSfenStr(QString* currentSfenStr);
    void setResumeSfenStr(QString* resumeSfenStr);
    void setOnMainRowGuard(bool* onMainRowGuard);

    // --------------------------------------------------------
    // コールバック設定
    // --------------------------------------------------------
    using ApplyEditMenuStateCallback = std::function<void(bool)>;
    using EnsurePositionEditCallback = std::function<void()>;

    void setApplyEditMenuStateCallback(ApplyEditMenuStateCallback cb);
    void setEnsurePositionEditCallback(EnsurePositionEditCallback cb);

    // --------------------------------------------------------
    // 編集用アクションの設定
    // --------------------------------------------------------
    struct EditActions {
        QAction* actionReturnAllPiecesToStand = nullptr;
        QAction* actionSetHiratePosition = nullptr;
        QAction* actionSetTsumePosition = nullptr;
        QAction* actionChangeTurn = nullptr;
    };
    void setEditActions(const EditActions& actions);

    // --------------------------------------------------------
    // 局面編集処理
    // --------------------------------------------------------

    /**
     * @brief 局面編集を開始
     */
    void beginPositionEditing();

public Q_SLOTS:
    /**
     * @brief 局面編集を終了
     */
    void finishPositionEditing();

private:
    ShogiView* m_shogiView = nullptr;
    ShogiGameController* m_gameController = nullptr;
    BoardInteractionController* m_boardController = nullptr;
    PositionEditController* m_posEdit = nullptr;
    MatchCoordinator* m_match = nullptr;
    ReplayController* m_replayController = nullptr;
    QStringList* m_sfenHistory = nullptr;

    const int* m_currentSelectedPly = nullptr;
    const int* m_activePly = nullptr;
    QString* m_startSfenStr = nullptr;
    QString* m_currentSfenStr = nullptr;
    QString* m_resumeSfenStr = nullptr;
    bool* m_onMainRowGuard = nullptr;

    EditActions m_editActions;

    // コールバック
    ApplyEditMenuStateCallback m_applyEditMenuState;
    EnsurePositionEditCallback m_ensurePositionEdit;
};

#endif // POSITIONEDITCOORDINATOR_H
