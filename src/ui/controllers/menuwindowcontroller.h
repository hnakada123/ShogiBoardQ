#ifndef MENUWINDOWCONTROLLER_H
#define MENUWINDOWCONTROLLER_H

/// @file menuwindowcontroller.h
/// @brief メニューウィンドウ（メニュー項目をボタンで並べる窓）の表示


#include <QObject>
#include <QList>

class MenuWindow;
class QWidget;
class QAction;
class QMenuBar;
class QMenu;

/**
 * @brief メニューバーの項目を集めてメニューウィンドウを作り、押されたボタンの項目を知らせる
 *
 * 責務:
 * - MenuWindowの遅延生成・表示管理
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
    struct Dependencies {
        QWidget* parentWidget = nullptr;
        QMenuBar* menuBar = nullptr;
    };

    /**
     * @brief コンストラクタ
     * @param deps 依存オブジェクト
     * @param parent 親オブジェクト
     */
    explicit MenuWindowController(const Dependencies& deps, QObject* parent = nullptr);

    /**
     * @brief デストラクタ
     */
    ~MenuWindowController() override = default;

    /**
     * @brief メニューウィンドウを表示する
     */
    void displayMenuWindow();

    /**
     * @brief MenuWindowを確保する（未作成時のみ作成）
     */
    void ensureMenuWindow();

    /**
     * @brief メニューウィンドウを取得
     * @return MenuWindowへのポインタ（未作成時はnullptr）
     */
    MenuWindow* menuWindow() const { return m_menuWindow; }

signals:
    /**
     * @brief アクションがトリガーされたシグナル
     * @param action トリガーされたアクション
     */
    void actionTriggered(QAction* action);

    /**
     * @brief お気に入りリストが変更されたシグナル
     * @param favorites 新しいお気に入りリスト
     */
    void favoritesChanged(const QStringList& favorites);

private:
    /**
     * @brief メニューバーからカテゴリ情報を収集
     */
    void collectCategoriesFromMenuBar();

    /**
     * @brief メニューからアクションを再帰的に収集
     * @param menu 対象メニュー
     * @param actions 収集先リスト
     */
    void collectActionsFromMenu(QMenu* menu, QList<QAction*>& actions);

    // 依存オブジェクト
    QWidget* m_parentWidget = nullptr;
    QMenuBar* m_menuBar = nullptr;

    // 内部オブジェクト
    MenuWindow* m_menuWindow = nullptr;
};

#endif // MENUWINDOWCONTROLLER_H
