#include "languagecontroller.h"
#include "settingsservice.h"

#include <QAction>
#include <QMessageBox>

LanguageController::LanguageController(QObject* parent)
    : QObject(parent)
    , m_actionGroup(new QActionGroup(this))
{
    m_actionGroup->setExclusive(true);
}

void LanguageController::setActions(QAction* systemAction, QAction* japaneseAction, QAction* englishAction)
{
    m_systemAction = systemAction;
    m_japaneseAction = japaneseAction;
    m_englishAction = englishAction;

    if (m_systemAction) {
        m_actionGroup->addAction(m_systemAction);
        m_systemAction->setIconVisibleInMenu(false);
    }
    if (m_japaneseAction) {
        m_actionGroup->addAction(m_japaneseAction);
        m_japaneseAction->setIconVisibleInMenu(false);
    }
    if (m_englishAction) {
        m_actionGroup->addAction(m_englishAction);
        m_englishAction->setIconVisibleInMenu(false);
    }
}

void LanguageController::setParentWidget(QWidget* parent)
{
    m_parentWidget = parent;
}

void LanguageController::updateMenuState()
{
    QString current = SettingsService::language();

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
    QString current = SettingsService::language();
    if (current == lang) return;

    SettingsService::setLanguage(lang);

    if (m_parentWidget) {
        QMessageBox::information(m_parentWidget,
            tr("言語設定"),
            tr("言語設定を変更しました。\n変更を反映するにはアプリケーションを再起動してください。"));
    }

    emit languageChanged(lang);
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
