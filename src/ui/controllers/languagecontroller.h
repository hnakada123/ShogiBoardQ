#ifndef LANGUAGECONTROLLER_H
#define LANGUAGECONTROLLER_H

/// @file languagecontroller.h
/// @brief 言語切替コントローラクラスの定義


#include <QObject>
#include <QActionGroup>

class QAction;
class QWidget;

/**
 * @brief 言語設定の管理を行うコントローラ
 *
 * MainWindowから分離された責務:
 * - 言語設定の変更
 * - 言語メニュー状態の更新
 * - 再起動要求ダイアログの表示
 */
class LanguageController : public QObject
{
    Q_OBJECT

public:
    explicit LanguageController(QObject* parent = nullptr);

    /**
     * @brief 言語アクションを設定
     * @param systemAction システム言語アクション
     * @param japaneseAction 日本語アクション
     * @param englishAction 英語アクション
     */
    void setActions(QAction* systemAction, QAction* japaneseAction, QAction* englishAction);

    /**
     * @brief 親ウィジェットを設定（ダイアログ表示用）
     */
    void setParentWidget(QWidget* parent);

    /**
     * @brief 言語メニューの状態を更新
     */
    void updateMenuState();

public slots:
    /**
     * @brief システム言語が選択された
     */
    void onSystemLanguageTriggered();

    /**
     * @brief 日本語が選択された
     */
    void onJapaneseTriggered();

    /**
     * @brief 英語が選択された
     */
    void onEnglishTriggered();

private:
    /**
     * @brief 言語を変更する
     * @param lang 言語コード（"system", "ja_JP", "en"）
     */
    void changeLanguage(const QString& lang);

    QActionGroup* m_actionGroup = nullptr;
    QAction* m_systemAction = nullptr;
    QAction* m_japaneseAction = nullptr;
    QAction* m_englishAction = nullptr;
    QWidget* m_parentWidget = nullptr;
};

#endif // LANGUAGECONTROLLER_H
