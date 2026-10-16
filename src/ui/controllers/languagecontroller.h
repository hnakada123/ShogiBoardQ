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
 * 責務:
 * - 言語設定の変更
 * - 言語メニュー状態の更新
 * - 再起動要求ダイアログの表示
 */
class LanguageController : public QObject
{
    Q_OBJECT

public:
    /// メニューの項目（どれも無くてよい）
    struct Deps {
        QWidget* parentWidget = nullptr;          ///< 再起動の知らせ・表記の説明の親
        QAction* systemLanguage = nullptr;
        QAction* japanese = nullptr;
        QAction* english = nullptr;
        QAction* chineseSimplified = nullptr;
        QAction* chineseTraditional = nullptr;
        QAction* notationAuto = nullptr;          ///< 指し手の表記: 言語に合わせる
        QAction* notationJapanese = nullptr;      ///< 指し手の表記: 日本式
        QAction* notationWestern = nullptr;       ///< 指し手の表記: 英語式
        QAction* notationOrigin = nullptr;        ///< 移動元を付ける
        QAction* notationHelp = nullptr;          ///< 表記の説明
    };

    /// 言語（相互排他）と表記の項目を設定とつなぐ
    explicit LanguageController(const Deps& deps, QObject* parent = nullptr);

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

private slots:
    void onChineseSimplifiedTriggered();
    void onChineseTraditionalTriggered();
    void onNotationTriggered(QAction* action);
    void onOriginTriggered(bool enabled);
    void showNotationHelp();

private:
    void setActions(QAction* systemAction, QAction* japaneseAction, QAction* englishAction,
                    QAction* simplifiedAction, QAction* traditionalAction);
    void setNotationActions(QAction* automatic, QAction* japanese, QAction* western, QAction* origin, QAction* help);
    void restartNotice();
    /**
     * @brief 言語を変更する
     * @param lang 言語コード（"system", "ja_JP", "en"）
     */
    void changeLanguage(const QString& lang);

    QActionGroup* m_actionGroup = nullptr;
    QAction* m_systemAction = nullptr;
    QAction* m_japaneseAction = nullptr;
    QAction* m_englishAction = nullptr;
    QAction* m_simplifiedAction = nullptr;
    QAction* m_traditionalAction = nullptr;
    QActionGroup* m_notationGroup = nullptr;
    QWidget* m_parentWidget = nullptr;
};

#endif // LANGUAGECONTROLLER_H
