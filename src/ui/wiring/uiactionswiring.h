#ifndef UIACTIONSWIRING_H
#define UIACTIONSWIRING_H

/// @file uiactionswiring.h
/// @brief メニュー/ツールバーアクションのシグナル/スロット接続ファサードの定義

#include <QObject>

namespace Ui { class MainWindow; }
class DialogLaunchWiring;
class DialogCoordinatorWiring;
class KifuFileController;
class FileActionsWiring;
class GameActionsWiring;
class EditActionsWiring;
class ViewActionsWiring;

/**
 * @brief メニューやツールバーのQActionをMainWindowスロットに接続するファサード
 *
 * UIアクション（ファイル操作、対局、盤操作、解析など）の
 * シグナル/スロット接続をドメイン別サブクラスへ委譲する。
 */
class UiActionsWiring : public QObject {
    Q_OBJECT
public:
    /// 依存オブジェクト
    struct Deps {
        Ui::MainWindow* ui = nullptr;       ///< UIオブジェクト
        QObject*        ctx = nullptr;       ///< 受け側オブジェクト（MainWindow*）
        DialogLaunchWiring* dlw = nullptr;   ///< ダイアログ起動配線
        DialogCoordinatorWiring* dcw = nullptr; ///< DialogCoordinator配線（解析中止用）
        KifuFileController* kfc = nullptr;      ///< 棋譜ファイル操作コントローラ
    };

    explicit UiActionsWiring(const Deps& d, QObject* parent=nullptr)
        : QObject(parent), m_d(d) {}

    /// 全アクションのシグナル/スロット接続を実行する（多重接続を避けるため1回だけ呼ぶ）
    void wire();

private:
    Deps m_d;
    FileActionsWiring* m_fileWiring = nullptr;
    GameActionsWiring* m_gameWiring = nullptr;
    EditActionsWiring* m_editWiring = nullptr;
    ViewActionsWiring* m_viewWiring = nullptr;
};

#endif // UIACTIONSWIRING_H
