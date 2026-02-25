#ifndef FILEACTIONSWIRING_H
#define FILEACTIONSWIRING_H

/// @file fileactionswiring.h
/// @brief ファイル/アプリ関連アクションの配線クラスの定義

#include <QObject>

namespace Ui { class MainWindow; }
class MainWindow;
class DialogLaunchWiring;
class KifuFileController;

/**
 * @brief ファイル/アプリ関連のQActionをMainWindowスロットに接続するクラス
 *
 * Quit, Save, Open, VersionInfo, Website, AboutQt の接続を担当する。
 */
class FileActionsWiring : public QObject {
    Q_OBJECT
public:
    struct Deps {
        Ui::MainWindow* ui = nullptr;
        MainWindow* mw = nullptr;
        DialogLaunchWiring* dlw = nullptr;
        KifuFileController* kfc = nullptr;  ///< 棋譜ファイル操作コントローラ
    };

    explicit FileActionsWiring(const Deps& d, QObject* parent = nullptr)
        : QObject(parent), m_d(d) {}

    void wire();

private:
    Deps m_d;
};

#endif // FILEACTIONSWIRING_H
