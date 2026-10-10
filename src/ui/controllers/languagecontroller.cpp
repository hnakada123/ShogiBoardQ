/// @file languagecontroller.cpp
/// @brief 言語切替コントローラクラスの実装

#include "languagecontroller.h"
#include "appsettings.h"
#include "dialogfontscale.h"

#include <QAction>
#include <QGuiApplication>
#include <QMessageBox>
#include <QScreen>
#include <QWidget>

LanguageController::LanguageController(QObject* parent)
    : QObject(parent)
    , m_actionGroup(new QActionGroup(this))
{
    m_actionGroup->setExclusive(true);
}

void LanguageController::setActions(QAction* systemAction, QAction* japaneseAction, QAction* englishAction,
                                    QAction* simplifiedAction, QAction* traditionalAction)
{
    m_systemAction = systemAction;
    m_japaneseAction = japaneseAction;
    m_englishAction = englishAction;

    if (m_systemAction) {
        m_actionGroup->addAction(m_systemAction);
        m_systemAction->setIconVisibleInMenu(false);
        connect(m_systemAction, &QAction::triggered, this, &LanguageController::onSystemLanguageTriggered);
    }
    if (m_japaneseAction) {
        m_actionGroup->addAction(m_japaneseAction);
        m_japaneseAction->setIconVisibleInMenu(false);
        connect(m_japaneseAction, &QAction::triggered, this, &LanguageController::onJapaneseTriggered);
    }
    if (m_englishAction) {
        m_actionGroup->addAction(m_englishAction);
        m_englishAction->setIconVisibleInMenu(false);
        connect(m_englishAction, &QAction::triggered, this, &LanguageController::onEnglishTriggered);
    }

    m_simplifiedAction = simplifiedAction;
    m_traditionalAction = traditionalAction;
    for (auto* action : {m_simplifiedAction, m_traditionalAction}) {
        if (!action) continue;
        m_actionGroup->addAction(action);
        action->setIconVisibleInMenu(false);
    }
    if (m_simplifiedAction) connect(m_simplifiedAction, &QAction::triggered, this, &LanguageController::onChineseSimplifiedTriggered);
    if (m_traditionalAction) connect(m_traditionalAction, &QAction::triggered, this, &LanguageController::onChineseTraditionalTriggered);
    // 初期状態を設定
    updateMenuState();
}

void LanguageController::setParentWidget(QWidget* parent)
{
    m_parentWidget = parent;
}

void LanguageController::updateMenuState()
{
    QString current = AppSettings::language();
    if (m_simplifiedAction) m_simplifiedAction->setChecked(current == "zh_CN");
    if (m_traditionalAction) m_traditionalAction->setChecked(current == "zh_TW");

    if (m_systemAction) {
        m_systemAction->setChecked(current == "system");
    }
    if (m_japaneseAction) {
        m_japaneseAction->setChecked(current == "ja_JP");
    }
    if (m_englishAction) {
        m_englishAction->setChecked(current == "en");
    }
}

void LanguageController::changeLanguage(const QString& lang)
{
    QString current = AppSettings::language();
    if (current == lang) return;

    AppSettings::setLanguage(lang);

    restartNotice();
}

void LanguageController::restartNotice()
{
    if (m_parentWidget) {
        QMessageBox::information(m_parentWidget,
            tr("言語設定"),
            tr("設定を変更しました。\n変更を反映するにはアプリケーションを再起動してください。"));
    }

}

void LanguageController::onSystemLanguageTriggered()
{
    changeLanguage("system");
    updateMenuState();
}

void LanguageController::onJapaneseTriggered()
{
    changeLanguage("ja_JP");
    updateMenuState();
}

void LanguageController::onEnglishTriggered()
{
    changeLanguage("en");
    updateMenuState();
}

void LanguageController::onChineseSimplifiedTriggered()
{
    changeLanguage(QStringLiteral("zh_CN"));
    updateMenuState();
}
void LanguageController::onChineseTraditionalTriggered()
{
    changeLanguage(QStringLiteral("zh_TW"));
    updateMenuState();
}
void LanguageController::setNotationActions(QAction* automatic, QAction* japanese, QAction* western, QAction* origin, QAction* help)
{
    connect(help, &QAction::triggered, this, &LanguageController::showNotationHelp);
    m_notationGroup = new QActionGroup(this);
    automatic->setData(QStringLiteral("auto"));
    japanese->setData(QStringLiteral("japanese"));
    western->setData(QStringLiteral("western"));
    for (auto* action : {automatic, japanese, western}) {
        m_notationGroup->addAction(action);
        action->setChecked(action->data().toString() == AppSettings::moveNotation());
    }
    connect(m_notationGroup, &QActionGroup::triggered, this, &LanguageController::onNotationTriggered);
    origin->setChecked(AppSettings::notationOrigin());
    connect(origin, &QAction::triggered, this, &LanguageController::onOriginTriggered);
}
void LanguageController::onNotationTriggered(QAction* action)
{
    if (AppSettings::moveNotation() == action->data().toString()) return;
    AppSettings::setMoveNotation(action->data().toString());
    restartNotice();
}
void LanguageController::onOriginTriggered(bool enabled)
{
    AppSettings::setNotationOrigin(enabled);
    restartNotice();
}

void LanguageController::showNotationHelp()
{
    QMessageBox box(QMessageBox::Information, tr("棋譜表記の読み方"),
        tr("表記設定は表示だけに適用されます。棋譜の保存形式は変わりません。\n\n"
           "英語表記：K=玉、R=飛、B=角、G=金、S=銀、N=桂、L=香、P=歩。\n"
           "駒名の前の + は成駒、手の末尾の + は成り、= は不成、x は駒取り、* は駒打ちです。\n"
           "例：▲P-7f、△Bx8h+、▲P*5e。筋は1〜9、段はa〜iです。\n"
           "移動元は区別が必要な場合に表示し、ツールチップでは常に確認できます。[+] は分岐を表します。\n\n"
           "日本語表記：同は直前と同じマス、成は成り、不成は成らない手、打は持ち駒を打つ手です。\n"
           "中国語の画面でも棋譜の駒名・記号は日本語表記を保持します。\n"
           "盤の反転でマスの座標は変わりません。駒画像は外観設定で選択できます。"),
        QMessageBox::Ok, m_parentWidget);
    // QMessageBox の既定の幅（約 500px）では「*」と「は駒打ちです」のように記号と説明が別の行に分かれるため、
    // 一番長い行が折り返さない幅にする（画面に収まる範囲で）。表示時に適用される文字サイズで測る
    int widest = 0;
    const QFontMetrics metrics(DialogFontScale::messageBoxFont(box.font()));
    const QStringList lines = box.text().split(QLatin1Char('\n'));
    for (const QString& line : lines) widest = qMax(widest, metrics.horizontalAdvance(line));
    if (const QScreen* screen = m_parentWidget ? m_parentWidget->screen() : QGuiApplication::primaryScreen())
        widest = qMin(widest, screen->availableGeometry().width() * 2 / 3);
    box.setStyleSheet(QStringLiteral("QLabel#qt_msgbox_label { min-width: %1px; }").arg(widest + 8));
    box.exec();
}
