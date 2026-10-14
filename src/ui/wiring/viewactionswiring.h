#ifndef VIEWACTIONSWIRING_H
#define VIEWACTIONSWIRING_H

/// @file viewactionswiring.h
/// @brief 盤操作・表示関連アクションの配線クラスの定義

#include <QObject>
#include <QPointer>

namespace Ui { class MainWindow; }
class MainWindowActions;
class QWidget;
class GameSessionOrchestrator;
class DialogLaunchWiring;
class MainWindowAppearanceController;
class ShogiView;
class EvaluationChartWidget;
class BoardColorDialog;

/**
 * @brief 盤操作・表示関連のQActionを担当の部品へ接続するクラス
 *
 * Flip, Enlarge, Shrink, CopyBoard, CopyEvalGraph,
 * ImmediateMove, MenuWindow, UndoMove, SaveBoardImage,
 * SaveEvalGraph, ToolBar の接続を担当する。
 */
class ViewActionsWiring : public QObject {
    Q_OBJECT
public:
    struct Deps {
        Ui::MainWindow* ui = nullptr;
        QWidget* window = nullptr;               ///< ダイアログの親
        MainWindowActions* actions = nullptr;    ///< 利用者操作の振り分け先
        DialogLaunchWiring* dlw = nullptr;
        GameSessionOrchestrator* gso = nullptr; ///< 対局ライフサイクルオーケストレータ
        MainWindowAppearanceController* appearance = nullptr; ///< UI外観コントローラ
        ShogiView* shogiView = nullptr;            ///< 将棋盤ビュー
        EvaluationChartWidget* evalChart = nullptr; ///< 評価値グラフウィジェット
    };

    explicit ViewActionsWiring(const Deps& d, QObject* parent = nullptr)
        : QObject(parent), m_d(d) {}

    void wire();

private slots:
    void copyBoardToClipboard();
    void copyEvalGraphToClipboard();
    void saveShogiBoardImage();
    void saveEvaluationGraphImage();
    void showBoardAppearance();
    void showFontSettings();

private:
    Deps m_d;
    QPointer<BoardColorDialog> m_boardColorDialog;
};

#endif // VIEWACTIONSWIRING_H
