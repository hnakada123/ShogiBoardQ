/// @file notationstyle.cpp
/// @brief 表示言語と棋譜の表記の設定の実装

#include "notationstyle.h"

#include <QLocale>

namespace NotationStyle {

namespace {
// 表示用の設定はメインスレッドで起動時に決める（configure を呼ぶのは起動時の ApplicationTranslations だけで、
// 表記の設定の変更は再起動で反映する。revision が変わるのはテストの中だけ）
Options current;
int currentRevision = 0;
} // namespace

QString resolveLanguage(const QString& setting, const QString& systemLocale)
{
    const QString requested = setting == QStringLiteral("system") ? systemLocale : setting;
    const QLocale locale(requested);
    if (locale.language() == QLocale::Japanese) return QStringLiteral("ja_JP");
    if (locale.language() == QLocale::Chinese)
        return locale.script() == QLocale::TraditionalHanScript ? QStringLiteral("zh_TW") : QStringLiteral("zh_CN");
    return QStringLiteral("en");
}

void configure(const QString& language, const QString& notation, bool alwaysOrigin)
{
    current.notation = notation == QStringLiteral("western")
            || (notation != QStringLiteral("japanese") && language == QStringLiteral("en"))
        ? Notation::Western
        : Notation::Japanese;
    current.alwaysOrigin = alwaysOrigin;
    ++currentRevision;
}

Options options()
{
    return current;
}

int revision()
{
    return currentRevision;
}

} // namespace NotationStyle
