#include "applicationfonts.h"

#include <QApplication>
#include <QFontDatabase>
#include <QEvent>
#include <QLocale>
#include <QPointer>
#include <QWidget>

#include <optional>
#include <utility>

namespace {

std::optional<QFont> defaultFont;
std::optional<QFont> systemFont;
QString selectedFamily;
QString uiLanguage;
QString japanese;
constexpr auto japaneseProperty = "shogiJapaneseFont";

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

QString chineseFamily(bool traditional)
{
    return firstInstalled(traditional ? QStringList{
#ifdef Q_OS_WIN
        QStringLiteral("Microsoft JhengHei UI"), QStringLiteral("Microsoft JhengHei"),
#elif defined(Q_OS_MACOS)
        QStringLiteral("PingFang TC"), QStringLiteral("Heiti TC"),
#endif
        QStringLiteral("Noto Sans CJK TC"), QStringLiteral("Noto Sans TC"),
        QStringLiteral("Source Han Sans TC")
    } : QStringList{
#ifdef Q_OS_WIN
        QStringLiteral("Microsoft YaHei UI"), QStringLiteral("Microsoft YaHei"),
#elif defined(Q_OS_MACOS)
        QStringLiteral("PingFang SC"), QStringLiteral("Heiti SC"),
#endif
        QStringLiteral("Noto Sans CJK SC"), QStringLiteral("Noto Sans SC"),
        QStringLiteral("Source Han Sans SC")
    });
}

QString japaneseMonospaceFamily()
{
    return firstInstalled({
#ifdef Q_OS_WIN
        QStringLiteral("MS Gothic"),
#elif defined(Q_OS_MACOS)
        QStringLiteral("Osaka-Mono"),
#endif
        QStringLiteral("Noto Sans Mono CJK JP"), QStringLiteral("IPAGothic"),
        QStringLiteral("IPAexGothic"), QStringLiteral("TakaoGothic")
    });
}

void appendFallback(QFont& font, const QString& family)
{
    if (family.isEmpty() || font.families().contains(family, Qt::CaseInsensitive)) return;
    font.setFamilies(font.families() + QStringList{family});
}

class JapaneseFontFilter : public QObject
{
public:
    using QObject::QObject;
protected:
    bool eventFilter(QObject* object, QEvent* event) override
    {
        if (event->type() == QEvent::FontChange && object->property(japaneseProperty).toBool()) {
            auto* widget = qobject_cast<QWidget*>(object);
            const QFont font = ApplicationFonts::japaneseFont(widget->font());
            if (font != widget->font()) widget->setFont(font);
        }
        return false;
    }
};

} // namespace

void ApplicationFonts::initialize(const QString& selected, const QString& language)
{
    // 言語の変更・再初期化でも、日本語化済みの書体をOS標準として保存しない。
    if (!systemFont) systemFont = QApplication::font();
    const QLocale locale(language);
    uiLanguage = locale.language() == QLocale::Japanese ? QStringLiteral("ja_JP")
        : locale.language() == QLocale::Chinese
            ? (locale.script() == QLocale::TraditionalHanScript ? QStringLiteral("zh_TW") : QStringLiteral("zh_CN"))
            : QStringLiteral("en");
    japanese = japaneseFamily();
    const QString family = uiLanguage == QLatin1String("ja_JP") ? japanese
        : uiLanguage.startsWith(QLatin1String("zh_")) ? chineseFamily(uiLanguage == QLatin1String("zh_TW")) : QString();
    defaultFont = *systemFont;
    if (!family.isEmpty()) defaultFont->setFamily(family);
    appendFallback(*defaultFont, japanese); // Qt 6.7でも、欧文UI内の日本語を補完する。

#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    // 漢字の補完先はUI言語に合わせる。仮名と棋譜は日本語の字形を使う。
    const QString han = uiLanguage == QLatin1String("en") ? japanese : family;
    QFontDatabase::setApplicationFallbackFontFamilies(QChar::Script_Han, han.isEmpty() ? QStringList{} : QStringList{han});
    for (const auto script : {QChar::Script_Hiragana, QChar::Script_Katakana})
        QFontDatabase::setApplicationFallbackFontFamilies(script, japanese.isEmpty() ? QStringList{} : QStringList{japanese});
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
    if (!selectedFamily.isEmpty()) {
        appFont.setFamily(selectedFamily);
        for (const auto& fallback : defaultFont->families()) appendFallback(appFont, fallback);
    }

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
        QFont base = selectedFamily.isEmpty() && font.styleHint() == QFont::Monospace
            ? monospaceFont() : appFont;
        if (entry.first->property(japaneseProperty).toBool()) base = japaneseFont(base);
        font.setFamilies(base.families());
        entry.first->setFont(font);
    }
}

QFont ApplicationFonts::monospaceFont()
{
    if (!defaultFont) initialize();
    if (!selectedFamily.isEmpty()) {
        QFont font = QApplication::font();
        font.setStyleHint(QFont::Monospace);
        return font;
    }
    QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    const QString family = uiLanguage == QLatin1String("ja_JP") ? japaneseMonospaceFamily()
        : uiLanguage == QLatin1String("zh_CN") ? firstInstalled({QStringLiteral("Noto Sans Mono CJK SC")})
        : uiLanguage == QLatin1String("zh_TW") ? firstInstalled({QStringLiteral("Noto Sans Mono CJK TC")}) : QString();
    if (!family.isEmpty()) {
        font.setFamily(family);
    } else {
        for (const auto& fallback : defaultFont->families()) appendFallback(font, fallback);
    }
    font.setStyleHint(QFont::Monospace);
    return font;
}

QFont ApplicationFonts::japaneseFont(QFont base)
{
    if (!defaultFont) initialize();
    if (!selectedFamily.isEmpty()) {
        base.setFamily(selectedFamily);
        appendFallback(base, japanese);
        return base;
    }
    QString family = base.styleHint() == QFont::Monospace ? japaneseMonospaceFamily() : japanese;
    if (family.isEmpty()) family = japanese;
    if (!family.isEmpty()) {
        const QStringList fallbacks = base.families();
        base.setFamily(family);
        for (const auto& fallback : fallbacks) appendFallback(base, fallback);
    }
    return base;
}

void ApplicationFonts::useJapaneseFont(QWidget* widget, bool enabled)
{
    static QPointer<JapaneseFontFilter> filter;
    if (!filter) filter = new JapaneseFontFilter(qApp);
    widget->setProperty(japaneseProperty, enabled);
    widget->installEventFilter(filter);
    QFont font = widget->font();
    if (enabled) font = japaneseFont(font);
    else font.setFamilies((font.styleHint() == QFont::Monospace ? monospaceFont() : QApplication::font()).families());
    widget->setFont(font);
}
