/// @file applicationtranslations.cpp
/// @brief 起動時の言語設定と翻訳ファイルの読み込み

#include "applicationtranslations.h"
#include "appsettings.h"
#include "kifupresentation.h"
#include "logcategories.h"

#include <QCoreApplication>
#include <QDir>
#include <QLibraryInfo>
#include <QLocale>

ApplicationTranslations::ApplicationTranslations(const QString& translationsDirectory)
{
    const QStringList systemLanguages = QLocale::system().uiLanguages();
    m_language = KifuPresentation::resolveLanguage(AppSettings::language(),
        systemLanguages.isEmpty() ? QLocale::system().name() : systemLanguages.first());

    const QString qtLanguage = m_language == QStringLiteral("ja_JP") ? QStringLiteral("ja") : m_language;
    if (m_qtTranslator.load(QStringLiteral(":/translations/qt/qtbase_") + qtLanguage + QStringLiteral(".qm"))
        || m_qtTranslator.load(QStringLiteral("qtbase_") + qtLanguage, QLibraryInfo::path(QLibraryInfo::TranslationsPath)))
        QCoreApplication::installTranslator(&m_qtTranslator);

    const QDir directory(translationsDirectory.isEmpty()
        ? QCoreApplication::applicationDirPath() : translationsDirectory);
    if (m_appTranslator.load(directory.filePath(QStringLiteral("ShogiBoardQ_") + m_language + QStringLiteral(".qm"))))
        QCoreApplication::installTranslator(&m_appTranslator);
    else
        qCWarning(lcApp) << "Translation file not found:" << m_language;

    KifuPresentation::configure(m_language, AppSettings::moveNotation(), AppSettings::notationOrigin());
}
