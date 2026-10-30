/// @file kifuformat.cpp
/// @brief 棋譜の形式の判定の実装

#include "kifuformat.h"
#include "kifutextdecoder.h"
#include "logcategories.h"

#include <QFileInfo>
#include <QRegularExpression>

namespace KifuFormats {

namespace {

struct Extension {
    KifuFormat format;
    const char* suffix;
    bool isName;            ///< 形式の名前（CLI・自動化 API の引数）。ほかは読むだけの別名・拡張子
    KifuEncoding encoding;  ///< 保存するときの文字コード
    const char* saveLabel;  ///< 保存ダイアログの絞り込みの見出し（保存しない拡張子は nullptr）
};

// 拡張子と形式・文字コード・保存の見出しの表。並びは保存ダイアログの絞り込みの順（UTF-8 の KIF が先頭）
const Extension kExtensions[] = {
    {KifuFormat::Kif, "kifu", false, KifuEncoding::Utf8, QT_TRANSLATE_NOOP("KifuFileFilter", "KIF形式 UTF-8")},
    {KifuFormat::Kif, "kif", true, KifuEncoding::ShiftJis, QT_TRANSLATE_NOOP("KifuFileFilter", "KIF形式 Shift_JIS")},
    {KifuFormat::Ki2, "ki2u", false, KifuEncoding::Utf8, QT_TRANSLATE_NOOP("KifuFileFilter", "KI2形式 UTF-8")},
    {KifuFormat::Ki2, "ki2", true, KifuEncoding::ShiftJis, QT_TRANSLATE_NOOP("KifuFileFilter", "KI2形式 Shift_JIS")},
    {KifuFormat::Csa, "csa", true, KifuEncoding::Utf8, QT_TRANSLATE_NOOP("KifuFileFilter", "CSA形式")},
    {KifuFormat::Jkf, "jkf", true, KifuEncoding::Utf8, QT_TRANSLATE_NOOP("KifuFileFilter", "JKF形式")},
    {KifuFormat::Usen, "usen", true, KifuEncoding::Utf8, QT_TRANSLATE_NOOP("KifuFileFilter", "USEN形式")},
    {KifuFormat::Usi, "usi", true, KifuEncoding::Utf8, QT_TRANSLATE_NOOP("KifuFileFilter", "USI形式")},
    {KifuFormat::Sfen, "sfen", true, KifuEncoding::Utf8, nullptr},
    {KifuFormat::Bod, "bod", true, KifuEncoding::Utf8, nullptr},
};

} // namespace

std::optional<KifuFormat> fromName(const QString& name)
{
    const QString key = name.trimmed().toLower();
    for (const auto& entry : kExtensions) {
        if (key == QLatin1String(entry.suffix)) return entry.format;
    }
    return std::nullopt;
}

QString name(KifuFormat format)
{
    for (const auto& entry : kExtensions) {
        if (entry.format == format && entry.isName) return QString::fromLatin1(entry.suffix);
    }
    return {};
}

std::optional<KifuFormat> fromPath(const QString& path)
{
    const QString suffix = QFileInfo(path).suffix();
    const auto format = suffix.isEmpty() ? std::nullopt : fromName(suffix);
    if (format == KifuFormat::Bod) return std::nullopt;
    return format;
}

bool isSavable(KifuFormat format)
{
    return format != KifuFormat::Sfen && format != KifuFormat::Bod;
}

QStringList savableNames()
{
    QStringList names;
    // 並びは形式の enum の順（"kif" "ki2" "csa" "jkf" "usi" "usen"）
    for (const KifuFormat format : {KifuFormat::Kif, KifuFormat::Ki2, KifuFormat::Csa, KifuFormat::Jkf,
                                    KifuFormat::Usi, KifuFormat::Usen})
        names.append(name(format));
    return names;
}

const QList<SaveTarget>& saveTargets()
{
    static const QList<SaveTarget> targets = [] {
        QList<SaveTarget> list;
        for (const auto& entry : kExtensions) {
            if (entry.saveLabel)
                list.append({QString::fromLatin1(entry.suffix), entry.format, entry.encoding, entry.saveLabel});
        }
        return list;
    }();
    return targets;
}

std::optional<SaveTarget> saveTargetForPath(const QString& path)
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix.isEmpty()) return std::nullopt;
    for (const SaveTarget& target : saveTargets()) {
        if (target.suffix == suffix) return target;
    }
    return std::nullopt;
}

QStringList readableSuffixes(std::optional<KifuFormat> format)
{
    QStringList suffixes;
    for (const auto& entry : kExtensions) {
        if (entry.format == KifuFormat::Bod) continue;   // 局面図はファイルで開かない
        if (!format || entry.format == *format) suffixes.append(QString::fromLatin1(entry.suffix));
        // JKF は .json で配布されることも多い
        if (entry.format == KifuFormat::Jkf && (!format || *format == KifuFormat::Jkf))
            suffixes.append(QStringLiteral("json"));
    }
    return suffixes;
}

bool isReadableFile(const QString& path)
{
    return readableSuffixes().contains(QFileInfo(path).suffix().toLower());
}

QStringList writableSuffixes()
{
    QStringList suffixes;
    for (const auto& entry : kExtensions) {
        if (entry.format != KifuFormat::Bod) suffixes.append(QString::fromLatin1(entry.suffix));
    }
    return suffixes;
}

KifuFormat detectFile(const QString& filePath)
{
    if (const auto format = fromPath(filePath)) return *format == KifuFormat::Sfen ? KifuFormat::Usi : *format;

    // JKF は .json で配布されることも多い。拡張子で決まらないときは内容で判定する。
    QStringList lines;
    QString usedEncoding;
    if (KifuTextDecoder::readAllLinesAuto(filePath, lines, &usedEncoding, nullptr)) {
        switch (const KifuFormat detected = detect(lines.join(QLatin1Char('\n')))) {
        case KifuFormat::Ki2:
        case KifuFormat::Csa:
        case KifuFormat::Jkf:
        case KifuFormat::Usen:
        case KifuFormat::Usi:
            return detected;
        case KifuFormat::Sfen:
            return KifuFormat::Usi;
        case KifuFormat::Kif:
        case KifuFormat::Bod:
            break;
        }
    }
    return KifuFormat::Kif;
}

KifuFormat detect(const QString& content)
{
    const QString trimmed = content.trimmed();

    // フォーマット判定用の正規表現（static で一度だけ構築）
    static const QRegularExpression sfenPattern(
        QStringLiteral("^[lnsgkrpbLNSGKRPB1-9+]+(/[lnsgkrpbLNSGKRPB1-9+]+){8}\\s+[bw]\\s+[-\\w]+\\s+\\d+$")
    );
    static const QRegularExpression usiLineRe(
        QStringLiteral("^\\s*(?:position\\b|startpos\\b|sfen\\s|[lnsgkrpb1-9+]+(?:/[lnsgkrpb1-9+]+){8}\\s+[bw]\\s)"),
        QRegularExpression::MultilineOption | QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression usenRe(
        QStringLiteral("^[a-zA-Z0-9_.-]*(?:~[0-9]*\\.[a-zA-Z0-9]*(?:\\.[a-zA-Z]?)?)+$"));
    static const QRegularExpression csaHeaderRe(
        QStringLiteral("^(?:V[23](?:\\.\\d+)?(?:,|$)|PI(?:[0-9]{2}[A-Z]{2})*(?:,|$)|P[1-9+-]|N[+-]|\\$(?:EVENT|SITE|TIME|START_TIME|END_TIME|NOTE):)"),
        QRegularExpression::MultilineOption);
    static const QRegularExpression csaLineStartRe(QStringLiteral("^[+-][0-9]"));
    static const QRegularExpression csaNewlineRe(QStringLiteral("\\n[+-][0-9]"));
    static const QRegularExpression kifMoveRe(QStringLiteral("^\\s*\\d+\\s+[０-９一二三四五六七八九同]"),
                                              QRegularExpression::MultilineOption);
    // BODの「手数＝4 △８四歩 まで」やコメント中の指し手をKI2と誤認しない。
    static const QRegularExpression ki2MoveRe(QStringLiteral("^\\s*[▲△▽☗☖][0-9０-９一二三四五六七八九同]"),
                                              QRegularExpression::MultilineOption);
    static const QRegularExpression bodBorderRe(QStringLiteral("^\\+[-─]+\\+"), QRegularExpression::MultilineOption);

    // SFEN判定
    if (sfenPattern.match(trimmed).hasMatch()) {
        qCDebug(lcKifu).noquote() << "detected format: SFEN";
        return KifuFormat::Sfen;
    }
    // JSON判定（JKF）
    if (trimmed.startsWith(QLatin1Char('{')) || trimmed.startsWith(QLatin1Char('['))) {
        qCDebug(lcKifu).noquote() << "detected format: JKF (JSON)";
        return KifuFormat::Jkf;
    }
    // USI判定
    if (usiLineRe.match(trimmed).hasMatch()) {
        qCDebug(lcKifu).noquote() << "detected format: USI";
        return KifuFormat::Usi;
    }
    // USEN全体の構造を確認し、コメントやURL内のチルダを除外する。
    if (usenRe.match(trimmed).hasMatch()) {
        qCDebug(lcKifu).noquote() << "detected format: USEN";
        return KifuFormat::Usen;
    }
    // 指し手がないV3・旧版の局面ファイルも判定する。
    if (csaHeaderRe.match(trimmed).hasMatch() ||
        trimmed.startsWith(QLatin1String("'")) ||
        csaLineStartRe.match(trimmed).hasMatch() ||
        content.contains(csaNewlineRe)) {
        qCDebug(lcKifu).noquote() << "detected format: CSA";
        return KifuFormat::Csa;
    }
    // KIF判定（"手数----" ヘッダまたは数字で始まる行）
    if (content.contains(QStringLiteral("手数----")) ||
        content.contains(kifMoveRe)) {
        qCDebug(lcKifu).noquote() << "detected format: KIF";
        return KifuFormat::Kif;
    }
    // KI2判定（▲△で始まる指し手）
    if (content.contains(ki2MoveRe)) {
        qCDebug(lcKifu).noquote() << "detected format: KI2";
        return KifuFormat::Ki2;
    }
    // BOD判定（局面図のみ: 指し手を含まない局面図）
    if (trimmed.contains(QStringLiteral("後手の持駒")) ||
        trimmed.contains(QStringLiteral("先手の持駒")) ||
        trimmed.contains(QStringLiteral("上手の持駒")) ||
        trimmed.contains(QStringLiteral("下手の持駒")) ||
        trimmed.contains(bodBorderRe) ||
        trimmed.contains(QStringLiteral("|v")) ||
        trimmed.contains(QStringLiteral("| ・"))) {
        qCDebug(lcKifu).noquote() << "detected format: BOD";
        return KifuFormat::Bod;
    }

    // 不明な場合はKIFとして試す
    qCDebug(lcKifu).noquote() << "format unknown, trying KIF";
    return KifuFormat::Kif;
}

} // namespace KifuFormats
