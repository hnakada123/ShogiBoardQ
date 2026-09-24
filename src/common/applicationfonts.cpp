#include "applicationfonts.h"

#include <QApplication>
#include <QFontDatabase>

namespace {

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

void ApplicationFonts::initialize()
{
    const QString family = japaneseFamily();
    if (family.isEmpty()) return;

    // システムの文字サイズ・太さを保ち、全ウィジェットの既定書体を揃える。
    // 英語 UI でも棋譜・プレイヤー名などに日本語を表示する。
    QFont font = QApplication::font();
    font.setFamily(family);
    QApplication::setFont(font);

#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    // 個別に欧文フォントを指定する表示でも、漢字・仮名は日本語用へ補完する。
    // Qt 6.7 は上の既定書体と monospaceFont() の明示指定で対応する。
    for (const auto script : {QChar::Script_Han, QChar::Script_Hiragana, QChar::Script_Katakana}) {
        QFontDatabase::addApplicationFallbackFontFamily(script, family);
    }
#endif
}

QFont ApplicationFonts::monospaceFont()
{
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
