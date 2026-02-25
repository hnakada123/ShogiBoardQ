#ifndef EDITACTIONSWIRING_H
#define EDITACTIONSWIRING_H

/// @file editactionswiring.h
/// @brief 編集メニュー関連アクションの配線クラスの定義

#include <QObject>

namespace Ui { class MainWindow; }
class MainWindow;
class DialogLaunchWiring;
class KifuExportController;
class KifuFileController;

/**
 * @brief 編集メニュー関連のQActionをスロットに接続するクラス
 *
 * 棋譜コピー（KIF,KI2,CSA,USI,JKF,USEN）、
 * 局面コピー（SFEN,BOD）、貼り付け、局面集ビューアの接続を担当する。
 */
class EditActionsWiring : public QObject {
    Q_OBJECT
public:
    struct Deps {
        Ui::MainWindow* ui = nullptr;
        MainWindow* mw = nullptr;
        KifuExportController* kec = nullptr;
        DialogLaunchWiring* dlw = nullptr;
        KifuFileController* kfc = nullptr;   ///< 棋譜ファイル操作コントローラ
    };

    explicit EditActionsWiring(const Deps& d, QObject* parent = nullptr)
        : QObject(parent), m_d(d) {}

    void wire();

private:
    Deps m_d;
};

#endif // EDITACTIONSWIRING_H
