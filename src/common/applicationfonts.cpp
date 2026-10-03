#include "applicationfonts.h"

#include <QApplication>
#include <QFontDatabase>
#include <QPointer>
#include <QWidget>

#include <optional>
#include <utility>

namespace {

std::optional<QFont> defaultFont;
QString selectedFamily;

QString firstInstalled(const QStringList& candidates)
{
    const QStringList available = QFontDatabase::families();
    for (const QString& family : candidates) {
        if (available.contains(family, Qt::CaseInsensitive)) return family;
    }
    return {};
}

QString japaneseFamily()
{
    // Japanese 対応の一覧には中国語・韓国語用の CJK フォントも入るため、
    // 日本語字形を持つファミリーを明示する。未導入なら OS の設定を維持する。
    return firstInstalled({
#ifdef Q_OS_WIN
        QStringLiteral("Yu Gothic UI"), QStringLiteral("Meiryo UI"),
        QStringLiteral("Meiryo"), QStringLiteral("MS UI Gothic"),
#elif defined(Q_OS_MACOS)
        QStringLiteral("Hiragino Sans"), QStringLiteral("Hiragino Kaku Gothic ProN"),
#endif
        QStringLiteral("Noto Sans CJK JP"), QStringLiteral("Noto Sans JP"),
        QStringLiteral("Source Han Sans JP"), QStringLiteral("IPAexGothic"),
        QStringLiteral("IPAGothic"), QStringLiteral("TakaoPGothic"),
        QStringLiteral("TakaoGothic")
    });
}

} // namespace

void ApplicationFonts::initialize(const QString& selected)
{
    const QString family = japaneseFamily();
    if (!defaultFont) {
        defaultFont = QApplication::font();
        if (!family.isEmpty()) defaultFont->setFamily(family);
    }

#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    // 個別に欧文フォントを指定する表示でも、漢字・仮名は日本語用へ補完する。
    // Qt 6.7 は上の既定書体と monospaceFont() の明示指定で対応する。
    if (!family.isEmpty()) {
        for (const auto script : {QChar::Script_Han, QChar::Script_Hiragana, QChar::Script_Katakana}) {
            QFontDatabase::addApplicationFallbackFontFamily(script, family);
        }
    }
#endif
    applyFamily(selected);
}

QString ApplicationFonts::defaultFamily()
{
    if (!defaultFont) initialize();
    return defaultFont->family();
}

void ApplicationFonts::applyFamily(const QString& family)
{
    if (!defaultFont) initialize();
    selectedFamily = firstInstalled({family});
    QFont appFont = *defaultFont;
    if (!selectedFamily.isEmpty()) appFont.setFamily(selectedFamily);

    // 個別の setFont() やスタイルシートがあると、QApplication の変更だけでは
    // 書体が伝播しない。親からの伝播前に全ウィジェットのサイズ・装飾を退避する。
    QList<std::pair<QPointer<QWidget>, QFont>> widgetFonts;
    const auto widgets = QApplication::allWidgets();
    for (QWidget* widget : widgets) {
        widgetFonts.append({widget, widget->font()});
    }
    QApplication::setFont(appFont);
    for (const auto& entry : widgetFonts) {
        if (!entry.first) continue;
        QFont font = entry.second;
        // 標準に戻したときは、通信ログなどの等幅表示も復元する。
        const QFont base = selectedFamily.isEmpty() && font.styleHint() == QFont::Monospace
            ? monospaceFont() : appFont;
        font.setFamilies(base.families());
        entry.first->setFont(font);
    }
}

QFont ApplicationFonts::monospaceFont()
{
    if (!selectedFamily.isEmpty()) {
        QFont font = QApplication::font();
        font.setStyleHint(QFont::Monospace);
        return font;
    }
    QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    const QString family = firstInstalled({
#ifdef Q_OS_WIN
        QStringLiteral("MS Gothic"),
#elif defined(Q_OS_MACOS)
        QStringLiteral("Osaka-Mono"),
#endif
        QStringLiteral("Noto Sans Mono CJK JP"), QStringLiteral("IPAGothic"),
        QStringLiteral("IPAexGothic"), QStringLiteral("TakaoGothic")
    });
    if (!family.isEmpty()) {
        font.setFamily(family);
    } else {
        const QString fallback = japaneseFamily();
        if (!fallback.isEmpty()) font.setFamilies(font.families() + QStringList{fallback});
    }
    font.setStyleHint(QFont::Monospace);
    return font;
}
