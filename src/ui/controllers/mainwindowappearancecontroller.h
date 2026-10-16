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
 * - セントラルウィジェット構築
 * - ツールバー設定・表示切替
 * - ツールチップ設定
 * - 盤面中央配置・サイズ追従
 * - 盤面・棋譜・ドック配置に合わせたウィンドウ横幅の固定
 * - 名前/時計フォント設定
 * - 盤反転+プレイヤー情報更新
 * - 盤拡大/縮小
 */
class MainWindowAppearanceController : public QObject
{
    Q_OBJECT

public:
    struct Deps {
        QMainWindow* window = nullptr;                  ///< 横幅を盤・ドックに合わせるウィンドウ（必須）
        QWidget* centralWidget = nullptr;               ///< 盤を置くセントラルウィジェット（必須）
        QToolBar* toolBar = nullptr;                    ///< ツールバー
        QAction* toolBarAction = nullptr;               ///< 「ツールバー」メニュー（表示状態を合わせる）
        ShogiView* shogiView = nullptr;                 ///< 盤（必須）
        TimeDisplayPresenter* timePresenter = nullptr;  ///< 盤を反転したときに時計を描き直す
    };

    /// セントラルウィジェットに盤を置き、ツールバーと盤の Ctrl+ホイールの拡大・縮小を設定する
    explicit MainWindowAppearanceController(const Deps& deps, QObject* parent = nullptr);

    /// ツールバーのボタンにアプリのツールチップを付ける（全部品ができてから1回呼ぶ）
    void installAppToolTips();

    // --- アクセサ ---

    QWidget* central() const { return m_central; }
public slots:
    /// 盤を反転する（手前に表示する側を入れ替え、駒の向きと時計の見た目を合わせる）
    void flipBoard();

    /// 盤拡大アクション
    void onActionEnlargeBoardTriggered();

    /// 盤縮小アクション
    void onActionShrinkBoardTriggered();

    /// ツールバー表示/非表示切替
    void onToolBarVisibilityToggled(bool visible);

private slots:
    /// 盤の大きさが変わったらセントラルウィジェットを盤に合わせる
    void fitCentralToBoard();

private:
    void setupCentralWidgetContainer(QWidget* centralWidget);
    void configureToolBarFromUi(QToolBar* toolBar, QAction* actionToolBar);
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
