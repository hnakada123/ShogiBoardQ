#ifndef MENUBUTTONWIDGET_H
#define MENUBUTTONWIDGET_H

/// @file menubuttonwidget.h
/// @brief メニューボタンウィジェットクラスの定義


#include <QWidget>
#include <QAction>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QDrag>

/**
 * @brief メニューウィンドウ用のボタンウィジェット
 *
 * アクションのアイコンとテキストを表示するボタン。
 * カスタマイズモード時には追加/削除ボタンを表示し、
 * ドラッグ&ドロップによる並べ替えに対応する。
 */
class MenuButtonWidget : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief コンストラクタ
     * @param action 関連付けるQAction
     * @param parent 親ウィジェット
     */
    explicit MenuButtonWidget(QAction* action, QWidget* parent = nullptr);

    /**
     * @brief 関連付けられているアクションを取得
     * @return QActionへのポインタ
     */
    QAction* action() const { return m_action; }

    /**
     * @brief アクションのobjectNameを取得
     * @return objectName文字列
     */
    QString actionName() const;

    /**
     * @brief カスタマイズモードを設定
     * @param enabled カスタマイズモードを有効にするかどうか
     * @param isFavoriteTab お気に入りタブかどうか
     * @param isInFavorites このアクションがお気に入りに登録済みかどうか
     */
    void setCustomizeMode(bool enabled, bool isFavoriteTab, bool isInFavorites);

    /**
     * @brief ボタンサイズを取得
     * @return 推奨サイズ
     */
    QSize sizeHint() const override;

    /**
     * @brief サイズ設定を更新
     * @param buttonSize ボタンサイズ（幅の基準値）
     * @param fontSize フォントサイズ
     * @param iconSize アイコンサイズ
     */
    void updateSizes(int buttonSize, int fontSize, int iconSize);

signals:
    /**
     * @brief アクションがトリガーされたときのシグナル
     * @param action トリガーされたアクション
     */
    void actionTriggered(QAction* action);

    /**
     * @brief お気に入りに追加要求シグナル
     * @param actionName アクションのobjectName
     */
    void addToFavorites(const QString& actionName);

    /**
     * @brief お気に入りから削除要求シグナル
     * @param actionName アクションのobjectName
     */
    void removeFromFavorites(const QString& actionName);

    /**
     * @brief ドラッグ開始シグナル
     * @param actionName ドラッグ中のアクション名
     */
    void dragStarted(const QString& actionName);

    /**
     * @brief ドロップ受け入れシグナル
     * @param sourceActionName ドラッグ元のアクション名
     * @param targetActionName ドロップ先のアクション名
     */
    void dropReceived(const QString& sourceActionName, const QString& targetActionName);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private slots:
    void onMainButtonClicked();
    void onAddButtonClicked();
    void onRemoveButtonClicked();
    void onActionChanged();

private:
    void setupUi();
    void updateButtonState();

    QAction* m_action = nullptr;
    QPushButton* m_mainButton = nullptr;
    QPushButton* m_addButton = nullptr;
    QPushButton* m_removeButton = nullptr;
    QLabel* m_iconLabel = nullptr;
    QLabel* m_textLabel = nullptr;
    QVBoxLayout* m_mainLayout = nullptr;

    bool m_customizeMode = false;
    bool m_isFavoriteTab = false;
    bool m_isInFavorites = false;
    QPoint m_dragStartPosition;

    // サイズ設定（動的に変更可能）
    int m_buttonWidth = 72;
    int m_buttonHeight = 64;
    int m_iconSize = 24;
    int m_fontSize = 9;
};

#endif // MENUBUTTONWIDGET_H
