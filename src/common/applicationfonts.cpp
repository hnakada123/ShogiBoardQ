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

constexpr auto japaneseProperty = "shogiJapaneseFont";

struct JapaneseFontEntry {
    QFont base;
    QFont result;
};

/// アプリ全体の書体の状態（メインスレッドだけから使う）
struct FontState {
    std::optional<QFont> defaultFont;
    std::optional<QFont> systemFont;
    QString selectedFamily;
    QString uiLanguage;
    QString japanese;
    // japaneseFont の結果の覚え。表のモデルが行を描くたび・列幅を測るたびに同じ書体で呼ぶので、
    // 書体の比較だけで返す。書体の設定（initialize / applyFamily）が変わるたびに捨てる。
    QList<JapaneseFontEntry> japaneseFontCache;
    int fontGeneration = 0;
    int japaneseFontCacheGeneration = -1;
};

Q_GLOBAL_STATIC(FontState, s_fonts)

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
    if (!s_fonts->systemFont) s_fonts->systemFont = QApplication::font();
    const QLocale locale(language);
    s_fonts->uiLanguage = locale.language() == QLocale::Japanese ? QStringLiteral("ja_JP")
        : locale.language() == QLocale::Chinese
            ? (locale.script() == QLocale::TraditionalHanScript ? QStringLiteral("zh_TW") : QStringLiteral("zh_CN"))
            : QStringLiteral("en");
    s_fonts->japanese = japaneseFamily();
    const QString family = s_fonts->uiLanguage == QLatin1String("ja_JP") ? s_fonts->japanese
        : s_fonts->uiLanguage.startsWith(QLatin1String("zh_")) ? chineseFamily(s_fonts->uiLanguage == QLatin1String("zh_TW")) : QString();
    s_fonts->defaultFont = *s_fonts->systemFont;
    if (!family.isEmpty()) s_fonts->defaultFont->setFamily(family);
    appendFallback(*s_fonts->defaultFont, s_fonts->japanese); // Qt 6.7でも、欧文UI内の日本語を補完する。

#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    // 漢字の補完先はUI言語に合わせる。仮名と棋譜は日本語の字形を使う。
    const QString han = s_fonts->uiLanguage == QLatin1String("en") ? s_fonts->japanese : family;
    QFontDatabase::setApplicationFallbackFontFamilies(QChar::Script_Han, han.isEmpty() ? QStringList{} : QStringList{han});
    for (const auto script : {QChar::Script_Hiragana, QChar::Script_Katakana})
        QFontDatabase::setApplicationFallbackFontFamilies(script, s_fonts->japanese.isEmpty() ? QStringList{} : QStringList{s_fonts->japanese});
#endif
    applyFamily(selected);
}

QString ApplicationFonts::defaultFamily()
{
    if (!s_fonts->defaultFont) initialize();
    return s_fonts->defaultFont->family();
}

void ApplicationFonts::applyFamily(const QString& family)
{
    if (!s_fonts->defaultFont) initialize();
    s_fonts->selectedFamily = firstInstalled({family});
    ++s_fonts->fontGeneration;
    QFont appFont = *s_fonts->defaultFont;
    if (!s_fonts->selectedFamily.isEmpty()) {
        appFont.setFamily(s_fonts->selectedFamily);
        const QStringList fallbacks = s_fonts->defaultFont->families();
        for (const auto& fallback : fallbacks) appendFallback(appFont, fallback);
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
        QFont base = s_fonts->selectedFamily.isEmpty() && font.styleHint() == QFont::Monospace
            ? monospaceFont() : appFont;
        if (entry.first->property(japaneseProperty).toBool()) base = japaneseFont(base);
        font.setFamilies(base.families());
        entry.first->setFont(font);
    }
}

QFont ApplicationFonts::monospaceFont()
{
    if (!s_fonts->defaultFont) initialize();
    if (!s_fonts->selectedFamily.isEmpty()) {
        QFont font = QApplication::font();
        font.setStyleHint(QFont::Monospace);
        return font;
    }
    QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    const QString family = s_fonts->uiLanguage == QLatin1String("ja_JP") ? japaneseMonospaceFamily()
        : s_fonts->uiLanguage == QLatin1String("zh_CN") ? firstInstalled({QStringLiteral("Noto Sans Mono CJK SC")})
        : s_fonts->uiLanguage == QLatin1String("zh_TW") ? firstInstalled({QStringLiteral("Noto Sans Mono CJK TC")}) : QString();
    if (!family.isEmpty()) {
        font.setFamily(family);
    } else {
        const QStringList fallbacks = s_fonts->defaultFont->families();
        for (const auto& fallback : fallbacks) appendFallback(font, fallback);
    }
    font.setStyleHint(QFont::Monospace);
    return font;
}

QFont ApplicationFonts::japaneseFont(QFont base)
{
    if (!s_fonts->defaultFont) initialize();
    if (s_fonts->japaneseFontCacheGeneration != s_fonts->fontGeneration) {
        s_fonts->japaneseFontCache.clear();
        s_fonts->japaneseFontCacheGeneration = s_fonts->fontGeneration;
    }
    // 結果はどの属性が指定済みか（resolveMask）にも依存する。QFont の比較はそれを見ないので
    // 別に比べる（指定済みでない属性は、表が自分の書体で補う）。
    for (const auto& entry : std::as_const(s_fonts->japaneseFontCache)) {
        if (entry.base == base && entry.base.resolveMask() == base.resolveMask()) return entry.result;
    }
    const QFont requested = base;
    if (!s_fonts->selectedFamily.isEmpty()) {
        base.setFamily(s_fonts->selectedFamily);
        appendFallback(base, s_fonts->japanese);
    } else {
        QString family = base.styleHint() == QFont::Monospace ? japaneseMonospaceFamily() : s_fonts->japanese;
        if (family.isEmpty()) family = s_fonts->japanese;
        if (!family.isEmpty()) {
            const QStringList fallbacks = base.families();
            base.setFamily(family);
            for (const auto& fallback : fallbacks) appendFallback(base, fallback);
        }
    }
    if (s_fonts->japaneseFontCache.size() >= 8) s_fonts->japaneseFontCache.removeFirst();
    s_fonts->japaneseFontCache.append({requested, base});
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
