#ifndef RECORDNAVIGATIONWIRING_H
#define RECORDNAVIGATIONWIRING_H

/// @file recordnavigationwiring.h
/// @brief 棋譜ナビゲーション配線クラスの定義

#include <QObject>

class BoardLoadService;
class ConsiderationPositionService;
class CsaGameWiring;
class EvaluationGraphController;
class JosekiWindowWiring;
class KifuBranchTree;
class KifuDisplayCoordinator;
class KifuNavigationCoordinator;
class KifuNavigationState;
class KifuRecordListModel;
class MatchCoordinator;
class RecordNavigationHandler;
class ShogiView;
class TurnStateSyncService;
class UiStatePolicyManager;
enum class PlayMode;

/**
 * @brief RecordNavigationHandler（棋譜欄の行変更）の生成・配線・依存設定
 *
 * wire() で1回だけハンドラを作り、通知を各部品へつなぐ。
 */
class RecordNavigationWiring : public QObject
{
    Q_OBJECT

public:
    /// ハンドラの依存（すべて非所有）
    struct Deps {
        KifuNavigationState* navState = nullptr;
        KifuBranchTree* branchTree = nullptr;
        KifuDisplayCoordinator* displayCoordinator = nullptr;
        KifuRecordListModel* kifuRecordModel = nullptr;
        ShogiView* shogiView = nullptr;
        EvaluationGraphController* evalGraphController = nullptr;
        QString* currentSfenStr = nullptr;              ///< AppState::currentSfenStr
        bool* skipBoardSyncForBranchNav = nullptr;      ///< AppState::skipBoardSyncForBranchNav
        CsaGameWiring* csaGameWiring = nullptr;
        PlayMode* playMode = nullptr;                   ///< AppState::playMode
        MatchCoordinator* match = nullptr;
    };

    /// ハンドラの通知の受け手（すべて非所有）
    struct WiringTargets {
        KifuNavigationCoordinator* kifuNav = nullptr;
        UiStatePolicyManager* uiStatePolicy = nullptr;
        ConsiderationPositionService* considerationPosition = nullptr;
        BoardLoadService* boardLoad = nullptr;
        TurnStateSyncService* turnSync = nullptr;
        JosekiWindowWiring* josekiWiring = nullptr;
    };

    explicit RecordNavigationWiring(QObject* parent = nullptr);

    /// ハンドラを作って依存を渡し、通知を受け手へつなぐ（1回だけ呼ぶ）
    void wire(const Deps& deps, const WiringTargets& targets);

    RecordNavigationHandler* handler() const { return m_handler; }

private:
    RecordNavigationHandler* m_handler = nullptr;
};

#endif // RECORDNAVIGATIONWIRING_H
