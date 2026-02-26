#ifndef GAMEACTIONSWIRING_H
#define GAMEACTIONSWIRING_H

/// @file gameactionswiring.h
/// @brief 対局/解析/検討/詰み関連アクションの配線クラスの定義

#include <QObject>

namespace Ui { class MainWindow; }
class GameSessionOrchestrator;
class MainWindow;
class DialogLaunchWiring;
class DialogCoordinatorWiring;

/**
 * @brief 対局・解析・検討・詰み関連のQActionをMainWindowスロットに接続するクラス
 *
 * NewGame, StartGame, CSA, Resign, BreakOff,
 * EngineSettings, Analyze, Tsume 等の接続を担当する。
 */
class GameActionsWiring : public QObject {
    Q_OBJECT
public:
    struct Deps {
        Ui::MainWindow* ui = nullptr;
        MainWindow* mw = nullptr;
        DialogLaunchWiring* dlw = nullptr;
        DialogCoordinatorWiring* dcw = nullptr; ///< 解析中止スロット接続先
        GameSessionOrchestrator* gso = nullptr; ///< 対局ライフサイクルオーケストレータ
    };

    explicit GameActionsWiring(const Deps& d, QObject* parent = nullptr)
        : QObject(parent), m_d(d) {}

    void wire();

private:
    Deps m_d;
};

#endif // GAMEACTIONSWIRING_H
