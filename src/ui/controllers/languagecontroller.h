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
    };

    /// 言語（相互排他）と表記の項目を設定とつなぐ
    explicit LanguageController(const Deps& deps, QObject* parent = nullptr);

    /**
     * @brief 言語メニューのチェックを保存した言語に合わせる
     */
    void updateMenuState();
    /// 移動元を付けるか（「移動元を付ける」のチェックの初期値）
    bool notationOrigin() const;

public slots:
    /// 「移動元を付ける」を変えた（保存し、再起動を案内する）
    void onOriginTriggered(bool enabled);
    /// 「棋譜表記の読み方」
    void showNotationHelp();

private slots:
    /// 言語の項目が選ばれた（項目の data が言語コード）
    void onLanguageTriggered(QAction* action);
    void onNotationTriggered(QAction* action);

private:
    void setNotationActions(QAction* automatic, QAction* japanese, QAction* western);
    void restartNotice();

    QActionGroup* m_languageGroup = nullptr;   ///< 言語の項目（data は "system"・"ja_JP"・"en"・"zh_CN"・"zh_TW"）
    QActionGroup* m_notationGroup = nullptr;
    QWidget* m_parentWidget = nullptr;
};

#endif // LANGUAGECONTROLLER_H
