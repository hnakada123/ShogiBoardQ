#ifndef POSITIONEDITCONTROLLER_H
#define POSITIONEDITCONTROLLER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <functional>
#include <QPointer>

class ShogiView;
class ShogiBoard;
class ShogiGameController;
class BoardInteractionController;

class PositionEditController final : public QObject {
    Q_OBJECT
public:
    explicit PositionEditController(QObject* parent = nullptr) : QObject(parent) {}

    // 局面編集開始に必要な情報
    struct BeginEditContext {
        ShogiView*                 view = nullptr;          // 必須
        ShogiGameController*       gc   = nullptr;          // 必須
        BoardInteractionController* bic = nullptr;          // 任意（ハイライト等）

        // SFEN 蓄積（0手局面～）の実体ポインタ
        QStringList* sfenRecord = nullptr;                  // 任意

        // 選択状態
        int  selectedPly = -1;                              // 任意
        int  activePly   = -1;                              // 任意
        bool gameOver    = false;                           // 任意

        // 文字列側の状態（存在すれば採用）
        QString* startSfenStr   = nullptr;                  // 任意
        QString* currentSfenStr = nullptr;                  // 任意
        QString* resumeSfenStr  = nullptr;                  // 任意

        // UI: 盤面右の「編集終了」ボタン表示
        std::function<void()> onShowEditExitButton;         // 任意
        std::function<void()> onEnterEditMenu;               // 追加: 編集開始時に呼ぶ
    };

    // 局面編集終了に必要な情報
    struct FinishEditContext {
        ShogiView*                 view = nullptr;          // 必須
        ShogiGameController*       gc   = nullptr;          // 必須
        BoardInteractionController* bic = nullptr;          // 任意

        QStringList* sfenRecord = nullptr;                  // 任意（0手局面として再保存）
        QString*     startSfenStr = nullptr;                // 任意（startSfen更新）
        bool*        isResumeFromCurrent = nullptr;         // 任意（falseに落とす）

        // UI: 「編集終了」ボタンの後片付け
        std::function<void()> onHideEditExitButton;         // 任意
        std::function<void()> onLeaveEditMenu;               // 追加: 編集終了時に呼ぶ
    };

    void beginPositionEditing(const BeginEditContext& c);
    void finishPositionEditing(const FinishEditContext& c);

    // ★ 追加: 盤操作と「編集終了」ボタン制御（アクション/開始時に直接呼ぶ）
    void resetPiecesToStand(ShogiView* view, BoardInteractionController* bic);
    void setStandardStartPosition(ShogiView* view, BoardInteractionController* bic);
    void setTsumeShogiStartPosition(ShogiView* view, BoardInteractionController* bic);

    void showEditExitButtonOnBoard(ShogiView* view, QObject* receiver, const char* finishSlot);
    void hideEditExitButtonOnBoard(ShogiView* view);

    // 局面編集中の「from→to」着手要求を適用する。
    // - 成功/失敗に関わらず view のドラッグ終了と bic.onMoveApplied(...) までを含めて処理します。
    // - 成功時のみ view->update() を呼びます。
    // 返り値: 成功すれば true、失敗すれば false。
    bool applyEditMove(const QPoint& from,
                       const QPoint& to,
                       ShogiView* view,
                       ShogiGameController* gc,
                       BoardInteractionController* bic);

    // class PositionEditController 内に追記（publicの下に）
public slots:
    void onReturnAllPiecesOnStandTriggered();
    void onFlatHandInitialPositionTriggered();
    void onShogiProblemInitialPositionTriggered();
    void onToggleSideToMoveTriggered();

private:
    QPointer<ShogiView>                 m_view;
    QPointer<ShogiGameController>       m_gc;
    QPointer<BoardInteractionController> m_bic;
};

#endif // POSITIONEDITCONTROLLER_H
