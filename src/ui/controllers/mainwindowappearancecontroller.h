#ifndef MAINWINDOWAPPEARANCECONTROLLER_H
#define MAINWINDOWAPPEARANCECONTROLLER_H

/// @file mainwindowappearancecontroller.h
/// @brief UI外観/ウィンドウ表示制御コントローラの定義

#include <QObject>

class ShogiView;
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
 * - セントラルウィジェットに盤を置き、盤の大きさの変化に合わせる
 * - ツールバーの設定と表示の切り替え、ツールバーのボタンのツールチップ
 * - 盤面・ドック配置に合わせたウィンドウ横幅の固定（全画面・最大化では外す）
 * - 盤の反転（駒の向きと時計の見た目を合わせる）と、メニューからの拡大・縮小
 *   （Ctrl+ホイールは ShogiView が自分で受ける）
 */
class MainWindowAppearanceController : public QObject
{
    Q_OBJECT

public:
    struct Deps {
        QMainWindow* window = nullptr;                  ///< 横幅を盤・ドックに合わせるウィンドウ（必須）
        QWidget* centralWidget = nullptr;               ///< 盤を置くセントラルウィジェット（必須）
        QToolBar* toolBar = nullptr;                    ///< ツールバー
        ShogiView* shogiView = nullptr;                 ///< 盤（必須）
        TimeDisplayPresenter* timePresenter = nullptr;  ///< 盤を反転したときに時計を描き直す
    };

    /// セントラルウィジェットに盤を置き、ツールバーを設定する
    explicit MainWindowAppearanceController(const Deps& deps, QObject* parent = nullptr);

    /// ツールバーのボタンにアプリのツールチップを付ける（全部品ができてから1回呼ぶ）
    void installAppToolTips();

    // --- アクセサ ---

    QWidget* central() const { return m_central; }
    /// ツールバーを出すか（保存した設定から始める。「ツールバー」のチェックの初期値）
    bool isToolBarVisible() const;

public slots:
    /// 盤を反転する（手前に表示する側を入れ替え、駒の向きと時計の見た目を合わせる）
    void flipBoard();

    /// 盤拡大アクション
    void onActionEnlargeBoardTriggered();

    /// 盤縮小アクション
    void onActionShrinkBoardTriggered();

    /// ツールバー表示/非表示切替（設定に保存する）
    void onToolBarVisibilityToggled(bool visible);

private slots:
    /// 盤の大きさが変わったらセントラルウィジェットを盤に合わせ、マスの大きさを保存する
    void onBoardZoomed();

private:
    /// セントラルウィジェットを盤の大きさに合わせる
    void fitCentralToBoard();
    void setupCentralWidgetContainer(QWidget* centralWidget);
    void configureToolBar(QToolBar* toolBar);
    void setupBoardInCenter();
    bool eventFilter(QObject* watched, QEvent* event) override;
    void scheduleWindowWidthUpdate();
    void updateWindowWidth();

    const Deps m_deps;
    QMainWindow* const m_mainWindow;
    QWidget* m_central = nullptr;
    QVBoxLayout* m_centralLayout = nullptr;
    bool m_windowWidthUpdatePending = false;
};

#endif // MAINWINDOWAPPEARANCECONTROLLER_H
