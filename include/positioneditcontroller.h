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

    struct BeginEditContext {
        ShogiGameController* gc = nullptr;                // 必須
        ShogiView*           view = nullptr;              // 必須
        BoardInteractionController* bic = nullptr;        // 任意（無ければ nullptr）

        // 既存の局面列（0手局面を含む）
        QStringList* sfenRecord = nullptr;

        // UI 状態
        int  selectedPly = -1;
        int  activePly   = -1;
        bool gameOver    = false;

        // 参照 SFEN 候補
        QString startSfen;
        QString currentSfen;
        QString resumeSfen;

        // UI 操作用コールバック（あれば実行）
        std::function<void()> onShowEditExitButton;
    };

    struct FinishEditContext {
        ShogiGameController* gc = nullptr;                 // 必須
        ShogiView*           view = nullptr;               // 必須
        BoardInteractionController* bic = nullptr;         // 任意

        QStringList* sfenRecord = nullptr;                 // 編集結果の 0手局面を書き戻す宛先
        QString*     startSfenStr = nullptr;               // 必要なら開始 SFEN に反映
        bool*        isResumeFromCurrent = nullptr;        // 再開フラグがあれば false に

        std::function<void()> onHideEditExitButton;        // UI 後片付け
    };

    void beginPositionEditing(const BeginEditContext& c);
    void finishPositionEditing(const FinishEditContext& c);
};

#endif // POSITIONEDITCONTROLLER_H
