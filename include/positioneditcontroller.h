#ifndef POSITIONEDITCONTROLLER_H
#define POSITIONEDITCONTROLLER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <functional>

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
    };

    void beginPositionEditing(const BeginEditContext& c);
    void finishPositionEditing(const FinishEditContext& c);
};

#endif // POSITIONEDITCONTROLLER_H
