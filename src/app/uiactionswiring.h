#ifndef UIACTIONSWIRING_H
#define UIACTIONSWIRING_H

/// @file uiactionswiring.h
/// @brief メニュー/ツールバーアクションのシグナル/スロット接続ファサードの定義

#include <QObject>
#include <utility>

namespace Ui { class MainWindow; }
class DialogLauncher;
class MainWindowActions;
class QWidget;
class DialogCoordinatorWiring;
class AnalysisSession;
class GameSession;
class KifuFileController;
class KifuExportController;
class MainWindowAppearanceController;
class ShogiView;
class EvaluationChartWidget;
class FileActionsWiring;
class GameActionsWiring;
class EditActionsWiring;
class ViewActionsWiring;

/**
 * @brief メニューやツールバーのQActionを担当の部品へ接続するファサード
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
        QWidget*        window = nullptr;    ///< ダイアログの親
        MainWindowActions* actions = nullptr; ///< 利用者操作の振り分け先
        DialogLauncher* dlw = nullptr;   ///< ダイアログ起動配線
        DialogCoordinatorWiring* dcw = nullptr; ///< DialogCoordinator配線（解析中止用）
        KifuFileController* kfc = nullptr;      ///< 棋譜ファイル操作コントローラ
        GameSession* session = nullptr;         ///< 対局のセッション
        AnalysisSession* analysis = nullptr;    ///< 検討・詰み探索のセッション
        MainWindowAppearanceController* appearance = nullptr; ///< UI外観コントローラ
        ShogiView* shogiView = nullptr;         ///< 将棋盤ビュー
        EvaluationChartWidget* evalChart = nullptr; ///< 評価値グラフウィジェット
        KifuExportController* kec = nullptr;    ///< 棋譜エクスポートコントローラ
    };

    explicit UiActionsWiring(Deps d, QObject* parent=nullptr)
        : QObject(parent), m_d(std::move(d)) {}

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
