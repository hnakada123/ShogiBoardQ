#ifndef MAINWINDOWAPPEARANCECONTROLLER_H
#define MAINWINDOWAPPEARANCECONTROLLER_H

/// @file mainwindowappearancecontroller.h
/// @brief UI外観/ウィンドウ表示制御コントローラの定義

#include <QObject>
#include <QSize>

class ShogiView;
class MatchCoordinator;
class TimeDisplayPresenter;
class QWidget;
class QVBoxLayout;
class QToolBar;
class QAction;
class QMainWindow;

/**
 * @brief UI外観/ウィンドウ表示制御を集約するコントローラ
 *
 * 責務:
 * - セントラルウィジェット構築
 * - ツールバー設定・表示切替
 * - ツールチップ設定
 * - 盤面中央配置・サイズ追従
 * - 名前/時計フォント設定
 * - 盤反転+プレイヤー情報更新
 * - 盤拡大/縮小
 * - タブ切替時の設定保存
 */
class MainWindowAppearanceController : public QObject
{
    Q_OBJECT

public:
    struct Deps {
        ShogiView** shogiView = nullptr;
        TimeDisplayPresenter** timePresenter = nullptr;
        MatchCoordinator** match = nullptr;
        bool* bottomIsP1 = nullptr;
        bool* lastP1Turn = nullptr;
        qint64* lastP1Ms = nullptr;
        qint64* lastP2Ms = nullptr;
    };

    explicit MainWindowAppearanceController(QObject* parent = nullptr);

    void updateDeps(const Deps& deps);

    // --- 初期化メソッド（コンストラクタ時に1回呼ぶ） ---

    /// セントラルウィジェットのコンテナを構築する
    void setupCentralWidgetContainer(QWidget* centralWidget);

    /// UIデザイナーで定義したツールバーを設定する
    void configureToolBarFromUi(QToolBar* toolBar, QAction* actionToolBar);

    /// アプリケーション全体のツールチップフィルタを設定する
    void installAppToolTips(QMainWindow* mainWindow);

    /// 将棋盤をセントラルウィジェットに配置する
    void setupBoardInCenter();

    /// 対局者名と時計のフォントを設定する
    void setupNameAndClockFonts();

    // --- アクセサ ---

    QWidget* central() const { return m_central; }
    QVBoxLayout* centralLayout() const { return m_centralLayout; }

public slots:
    /// 盤面サイズ変更時の後処理
    void onBoardSizeChanged(QSize fieldSize);

    /// 評価値グラフの高さ調整を遅延実行する
    void performDeferredEvalChartResize();

    /// 盤反転+プレイヤー情報更新
    void flipBoardAndUpdatePlayerInfo();

    /// 盤面反転完了後の後処理
    void onBoardFlipped(bool nowFlipped);

    /// 盤反転アクション
    void onActionFlipBoardTriggered();

    /// 盤拡大アクション
    void onActionEnlargeBoardTriggered();

    /// 盤縮小アクション
    void onActionShrinkBoardTriggered();

    /// ツールバー表示/非表示切替
    void onToolBarVisibilityToggled(bool visible);

    /// タブ切替時の設定保存
    void onTabCurrentChanged(int index);

private:
    Deps m_deps;
    QWidget* m_central = nullptr;
    QVBoxLayout* m_centralLayout = nullptr;
    QToolBar* m_toolBar = nullptr;
};

#endif // MAINWINDOWAPPEARANCECONTROLLER_H
