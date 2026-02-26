#ifndef VIEWACTIONSWIRING_H
#define VIEWACTIONSWIRING_H

/// @file viewactionswiring.h
/// @brief 盤操作・表示関連アクションの配線クラスの定義

#include <QObject>

namespace Ui { class MainWindow; }
class GameSessionOrchestrator;
class MainWindow;
class DialogLaunchWiring;
class MainWindowAppearanceController;
class ShogiView;
class EvaluationChartWidget;

/**
 * @brief 盤操作・表示関連のQActionをMainWindowスロットに接続するクラス
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
        MainWindow* mw = nullptr;
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

private:
    Deps m_d;
};

#endif // VIEWACTIONSWIRING_H
