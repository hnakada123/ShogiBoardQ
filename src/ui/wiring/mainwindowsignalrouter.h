#ifndef MAINWINDOWSIGNALROUTER_H
#define MAINWINDOWSIGNALROUTER_H

/// @file mainwindowsignalrouter.h
/// @brief MainWindow のシグナル配線を集約するルーター

#include <QObject>

class MainWindow;

/**
 * @brief MainWindow のシグナル配線を集約するルーター
 *
 * 責務:
 * - コンストラクタ時の全シグナル/スロット接続を一括管理
 * - ダイアログ起動配線・アクション配線・コアシグナル配線を connectAll() に集約
 * - MatchCoordinator 初期化時の盤面クリック・着手要求配線
 *
 * MainWindow の private メンバへアクセスするため friend 宣言が必要。
 */
class MainWindowSignalRouter : public QObject
{
    Q_OBJECT

public:
    explicit MainWindowSignalRouter(MainWindow& mw, QObject* parent = nullptr);

    /// 全配線を実行する（コンストラクタから1回だけ呼ぶ）
    void connectAll();

    /// MatchCoordinator 初期化時の盤面クリック配線
    void connectBoardClicks();

    /// MatchCoordinator 初期化時の着手要求配線
    void connectMoveRequested();

private:
    /// ダイアログ起動配線を初期化する
    void initializeDialogLaunchWiring();

    /// 全メニューアクションのシグナル/スロットを接続する
    void connectAllActions();

    /// コアシグナル群を接続する
    void connectCoreSignals();

    MainWindow& m_mw;
};

#endif // MAINWINDOWSIGNALROUTER_H
