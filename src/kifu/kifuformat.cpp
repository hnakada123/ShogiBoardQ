/// @file kifuformat.cpp
/// @brief 棋譜の形式の判定の実装

#include "kifuformat.h"
#include "kifreader.h"
#include "logcategories.h"

#include <QFileInfo>
#include <QRegularExpression>

namespace KifuFormats {

namespace {

struct FormatName {
    KifuFormat format;
    const char* name;
};

// 名前・拡張子。形式ごとに最初の行が名前（後ろの行は読むだけの別名・拡張子）
const FormatName kNames[] = {
    {KifuFormat::Kif, "kif"},
    {KifuFormat::Ki2, "ki2"},
    {KifuFormat::Csa, "csa"},
    {KifuFormat::Jkf, "jkf"},
    {KifuFormat::Usi, "usi"},
    {KifuFormat::Usen, "usen"},
    {KifuFormat::Sfen, "sfen"},
    {KifuFormat::Bod, "bod"},
    {KifuFormat::Kif, "kifu"},
    {KifuFormat::Ki2, "ki2u"},
};

} // namespace

std::optional<KifuFormat> fromName(const QString& name)
{
    const QString key = name.trimmed().toLower();
    for (const auto& entry : kNames) {
        if (key == QLatin1String(entry.name)) return entry.format;
    }
    return std::nullopt;
}

QString name(KifuFormat format)
{
    for (const auto& entry : kNames) {
        if (entry.format == format) return QString::fromLatin1(entry.name);
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

KifuFormat detectFile(const QString& filePath)
{
    if (const auto format = fromPath(filePath)) return *format == KifuFormat::Sfen ? KifuFormat::Usi : *format;

    // JKF は .json で配布されることも多い。拡張子で決まらないときは内容で判定する。
    QStringList lines;
    QString usedEncoding;
    if (KifReader::readLinesAuto(filePath, lines, &usedEncoding, nullptr)) {
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
