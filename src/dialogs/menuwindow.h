#ifndef MENUWINDOW_H
#define MENUWINDOW_H

#include <QWidget>
#include <QTabWidget>
#include <QToolButton>
#include <QGridLayout>
#include <QScrollArea>
#include <QAction>
#include <QList>
#include <QMap>
#include <QStringList>

class MenuButtonWidget;

/**
 * @brief メニューウィンドウクラス
 *
 * GUIメニューの各項目をボタン形式で表示するモードレスウィンドウ。
 * タブ付きパネル（★お気に入り + カテゴリ別）とカスタマイズモード切替を実装。
 */
class MenuWindow : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief カテゴリ情報の構造体
     */
    struct CategoryInfo {
        QString name;           ///< カテゴリ名
        QString displayName;    ///< 表示名
        QList<QAction*> actions;///< カテゴリに属するアクション
    };

    /**
     * @brief コンストラクタ
     * @param parent 親ウィジェット
     */
    explicit MenuWindow(QWidget* parent = nullptr);

    /**
     * @brief デストラクタ
     */
    ~MenuWindow() override = default;

    /**
     * @brief カテゴリとアクションを設定
     * @param categories カテゴリ情報のリスト
     */
    void setCategories(const QList<CategoryInfo>& categories);

    /**
     * @brief お気に入りリストを設定
     * @param favoriteActionNames お気に入りアクションのobjectNameリスト
     */
    void setFavorites(const QStringList& favoriteActionNames);

    /**
     * @brief お気に入りリストを取得
     * @return お気に入りアクションのobjectNameリスト
     */
    QStringList favorites() const { return m_favoriteActionNames; }

signals:
    /**
     * @brief アクションがトリガーされたときのシグナル
     * @param action トリガーされたアクション
     */
    void actionTriggered(QAction* action);

    /**
     * @brief お気に入りリストが変更されたときのシグナル
     * @param favoriteActionNames 新しいお気に入りリスト
     */
    void favoritesChanged(const QStringList& favoriteActionNames);

protected:
    /**
     * @brief ウィンドウを閉じる際の処理
     */
    void closeEvent(QCloseEvent* event) override;

private slots:
    /**
     * @brief カスタマイズモード切替ボタンがクリックされたときのスロット
     */
    void onCustomizeButtonClicked();

    /**
     * @brief アクションがトリガーされたときのスロット
     * @param action トリガーされたアクション
     */
    void onActionTriggered(QAction* action);

    /**
     * @brief お気に入りに追加要求時のスロット
     * @param actionName アクションのobjectName
     */
    void onAddToFavorites(const QString& actionName);

    /**
     * @brief お気に入りから削除要求時のスロット
     * @param actionName アクションのobjectName
     */
    void onRemoveFromFavorites(const QString& actionName);

    /**
     * @brief ドラッグ&ドロップによる並べ替え時のスロット
     * @param sourceActionName ドラッグ元のアクション名
     * @param targetActionName ドロップ先のアクション名
     */
    void onFavoriteReordered(const QString& sourceActionName, const QString& targetActionName);

    /**
     * @brief タブが変更されたときのスロット
     * @param index 新しいタブインデックス
     */
    void onTabChanged(int index);

    /**
     * @brief ボタンサイズを拡大
     */
    void onButtonSizeIncrease();

    /**
     * @brief ボタンサイズを縮小
     */
    void onButtonSizeDecrease();

    /**
     * @brief フォントサイズを拡大
     */
    void onFontSizeIncrease();

    /**
     * @brief フォントサイズを縮小
     */
    void onFontSizeDecrease();

private:
    /**
     * @brief UIのセットアップ
     */
    void setupUi();

    /**
     * @brief カテゴリタブを作成
     * @param category カテゴリ情報
     * @return 作成されたスクロールエリア
     */
    QScrollArea* createCategoryTab(const CategoryInfo& category);

    /**
     * @brief お気に入りタブを更新
     */
    void updateFavoritesTab();

    /**
     * @brief 全タブのカスタマイズモードを更新
     */
    void updateCustomizeModeForAllTabs();

    /**
     * @brief ボタンをグリッドに配置
     * @param layout グリッドレイアウト
     * @param actions アクションリスト
     * @param isFavoriteTab お気に入りタブかどうか
     */
    void populateGrid(QGridLayout* layout, const QList<QAction*>& actions, bool isFavoriteTab);

    /**
     * @brief アクション名からアクションを取得
     * @param actionName アクションのobjectName
     * @return QActionへのポインタ（見つからない場合はnullptr）
     */
    QAction* findActionByName(const QString& actionName) const;

    /**
     * @brief 設定を読み込む
     */
    void loadSettings();

    /**
     * @brief 設定を保存する
     */
    void saveSettings();

    /**
     * @brief 全ボタンのサイズを更新
     */
    void updateAllButtonSizes();

    // UI要素
    QTabWidget* m_tabWidget = nullptr;
    QToolButton* m_customizeButton = nullptr;
    QToolButton* m_buttonSizeIncreaseBtn = nullptr;
    QToolButton* m_buttonSizeDecreaseBtn = nullptr;
    QToolButton* m_fontSizeIncreaseBtn = nullptr;
    QToolButton* m_fontSizeDecreaseBtn = nullptr;
    QScrollArea* m_favoritesScrollArea = nullptr;
    QWidget* m_favoritesContainer = nullptr;
    QGridLayout* m_favoritesLayout = nullptr;

    // データ
    QList<CategoryInfo> m_categories;
    QStringList m_favoriteActionNames;
    QMap<QString, QAction*> m_actionMap;  // objectName -> QAction
    QList<MenuButtonWidget*> m_allButtons;

    // 状態
    bool m_customizeMode = false;

    // サイズ設定
    int m_buttonSize = 72;   // ボタンサイズ
    int m_fontSize = 9;      // フォントサイズ
    int m_iconSize = 24;     // アイコンサイズ

    // 定数
    static constexpr int kColumnsPerRow = 5;
    static constexpr int kMinButtonSize = 48;
    static constexpr int kMaxButtonSize = 120;
    static constexpr int kMinFontSize = 7;
    static constexpr int kMaxFontSize = 24;
};

#endif // MENUWINDOW_H
