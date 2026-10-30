#ifndef MENUWINDOWCONTROLLER_H
#define MENUWINDOWCONTROLLER_H

/// @file menuwindowcontroller.h
/// @brief メニューウィンドウ（メニュー項目をボタンで並べる窓）の表示


#include <QObject>
#include <QList>

class MenuWindow;
class QDockWidget;
class QWidget;
class QAction;
class QMenuBar;
class QMenu;

/**
 * @brief メニューバーの項目を集めてメニューウィンドウを作り、押されたボタンの項目を知らせる
 *
 * 責務:
 * - MenuWindow の生成と、ドックの表示の切り替え（ツールバーの「メニュー」）
 * - メニューバーからのアクション収集
 * - カテゴリ情報の構築
 */
class MenuWindowController : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 依存オブジェクト構造体
     */
    struct Deps {
        QWidget* parentWidget = nullptr;
        QMenuBar* menuBar = nullptr;
    };

    /**
     * @brief コンストラクタ
     * @param deps 依存オブジェクト
     * @param parent 親オブジェクト
     */
    explicit MenuWindowController(const Deps& deps, QObject* parent = nullptr);

    /**
     * @brief デストラクタ
     */
    ~MenuWindowController() override = default;

    /**
     * @brief メニューウィンドウを取得
     * @return MenuWindowへのポインタ（コンストラクタで作る。ドックに入れると所有はドックに移る）
     */
    MenuWindow* menuWindow() const { return m_menuWindow; }

    /// メニューバーの項目からメニューウィンドウの分類を作り直す（メニューに項目を足した後に呼ぶ）
    void collectCategoriesFromMenuBar();
    /// メニューウィンドウを入れたドック（表示の切り替えに使う）
    void attachDock(QDockWidget* dock) { m_dock = dock; }

public slots:
    /// メニューウィンドウのドックの表示を切り替える。切り替えはドックの toggleViewAction（「表示」メニューの項目と
    /// 同じ）に任せ、出したときは重ねたタブの前面にする
    void toggleWindow();

private:
    /**
     * @brief メニューからアクションを再帰的に収集
     * @param menu 対象メニュー
     * @param actions 収集先リスト
     */
    void collectActionsFromMenu(QMenu* menu, QList<QAction*>& actions);

    // 依存オブジェクト
    QMenuBar* m_menuBar = nullptr;

    // 内部オブジェクト
    MenuWindow* m_menuWindow = nullptr;
    QDockWidget* m_dock = nullptr;   ///< 非所有
};

#endif // MENUWINDOWCONTROLLER_H
