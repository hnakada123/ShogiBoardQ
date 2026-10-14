#ifndef TURNSTATESYNCSERVICE_H
#define TURNSTATESYNCSERVICE_H

/// @file turnstatesyncservice.h
/// @brief 手番同期サービス — TurnManager / GameController / View / 時計の手番同期を集約

#include <QObject>
#include <QPointer>
#include <functional>

#include "shogigamecontroller.h"

class ShogiClock;
class ShogiView;
class TimeDisplayPresenter;
class TurnManager;

/**
 * @brief 手番の同期をまとめる
 *
 * 手番は GameController（対局中の正）と TurnManager（表示の通知元。このサービスが持つ）が持つ。
 * GC の手番変更 → TurnManager → 時計・盤・手番表示 とつなぐ。
 */
class TurnStateSyncService : public QObject
{
    Q_OBJECT

public:
    struct Deps {
        ShogiGameController* gameController = nullptr;
        ShogiView* shogiView = nullptr;
        ShogiClock* clock = nullptr;
        TimeDisplayPresenter* timePresenter = nullptr;
        /// 対局が進行中か（対局中は GC、それ以外は盤面 SFEN の手番を正とする）
        std::function<bool()> isGameActivelyInProgress;
    };

    explicit TurnStateSyncService(QObject* parent = nullptr);

    /// 依存を設定し、GC の手番変更を TurnManager へつなぐ
    void updateDeps(const Deps& deps);

    TurnManager* turnManager() const { return m_turnManager; }

    /// 手番表示と時計の表示を更新する
    void updateTurnAndTimekeepingDisplay();

public slots:
    /// 対局中は GC、それ以外は盤面 SFEN の手番を正として TurnManager と GC を合わせる
    void setCurrentTurn();
    /// TurnManager の手番変更を時計・GC・手番表示へ反映する
    void onTurnManagerChanged(Turn now);

private:
    Deps m_deps;
    TurnManager* m_turnManager = nullptr;          ///< this が所有
    QPointer<ShogiGameController> m_connectedGc;   ///< 手番変更をつないでいる GC
};

#endif // TURNSTATESYNCSERVICE_H
