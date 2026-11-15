#ifndef MAINWINDOWAPPEARANCECONTROLLER_H
#define MAINWINDOWAPPEARANCECONTROLLER_H

/// @file mainwindowappearancecontroller.h
/// @brief UI外観/ウィンドウ表示制御コントローラの定義

#include <QObject>
#include <QSizePolicy>

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
 * - 盤面・ドック配置に合わせたウィンドウ横幅の固定（全画面・最大化では外す）。ドックをドラッグしている間は
 *   ウィンドウの大きさを変えず、盤の領域を縮めてよいことにして、ドックを盤の横・下段・上端へ入れられるようにする
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

    /// ドックのドラッグが始まる: 終わるまでウィンドウの大きさを変えず、盤の領域を縮めてよいことにする
    /// （Qt が、盤の横・下段・上端をドロップ先にできる）
    void beginDockDrag();
    /// ドックのドラッグが終わった: 盤の領域を元に戻し、今の配置に必要な大きさ（横幅は固定、高さは足りなければ）にする
    void endDockDrag();

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
    /// 画面いっぱいに広げているか（全画面・最大化。横幅を配置に合わせて固定しない）
    bool fillsScreen() const;
    void scheduleWindowSizeUpdate();
    /// 横幅を今の配置に必要な幅に固定する。高さは、今の配置に足りないときだけ必要な高さにする
    void updateWindowSize();

    const Deps m_deps;
    QMainWindow* const m_mainWindow;
    QWidget* m_central = nullptr;
    QVBoxLayout* m_centralLayout = nullptr;
    bool m_windowSizeUpdatePending = false;
    bool m_dockDragActive = false;  ///< ドックをドラッグしている間（ウィンドウの大きさを変えず、盤の領域を縮められる）
    bool m_dockAnimationToRestore = true;  ///< ドラッグの前の、ドックのアニメーションの設定
    QSizePolicy m_centralPolicyToRestore;  ///< ドラッグの前の、盤の領域の大きさの方針
    /// 決めた大きさと違う大きさにされたときに、続けて合わせ直した回数（合うか、固定する幅が変わると 0 に戻す）
    int m_sizeCorrections = 0;
    static constexpr int kMaxSizeCorrections = 5;
};

#endif // MAINWINDOWAPPEARANCECONTROLLER_H
