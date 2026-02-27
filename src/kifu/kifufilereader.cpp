/// @file kifufilereader.cpp
/// @brief 棋譜ファイル読み込みI/O層の実装

#include "kifufilereader.h"
#include "logcategories.h"

#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QStringConverter>

namespace KifuFileReader {

KifuFormat detectFormat(const QString& content)
{
    const QString trimmed = content.trimmed();

    // フォーマット判定用の正規表現（static で一度だけ構築）
    static const QRegularExpression sfenPattern(
        QStringLiteral("^(sfen\\s+)?[lnsgkrpbLNSGKRPB1-9+]+(/[lnsgkrpbLNSGKRPB1-9+]+){8}\\s+[bw]\\s+[-\\w]+\\s+\\d+")
    );
    static const QRegularExpression csaLineStartRe(QStringLiteral("^[+-][0-9]"));
    static const QRegularExpression csaNewlineRe(QStringLiteral("\\n[+-][0-9]"));
    static const QRegularExpression kifMoveRe(QStringLiteral("^\\s*\\d+\\s+[０-９一二三四五六七八九同]"));
    static const QRegularExpression ki2MoveRe(QStringLiteral("[▲△][０-９一二三四五六七八九同]"));
    static const QRegularExpression bodBorderRe(QStringLiteral("^\\+[-─]+\\+"), QRegularExpression::MultilineOption);

    // SFEN判定
    if (sfenPattern.match(trimmed).hasMatch()) {
        qCDebug(lcKifu).noquote() << "detected format: SFEN";
        return KifuFormat::SFEN;
    }
    // JSON判定（JKF）
    if (trimmed.startsWith(QLatin1Char('{'))) {
        qCDebug(lcKifu).noquote() << "detected format: JKF (JSON)";
        return KifuFormat::JKF;
    }
    // USI判定
    if (trimmed.startsWith(QLatin1String("position"))) {
        qCDebug(lcKifu).noquote() << "detected format: USI";
        return KifuFormat::USI;
    }
    // USEN判定（チルダを含む）
    if (trimmed.contains(QLatin1Char('~'))) {
        qCDebug(lcKifu).noquote() << "detected format: USEN";
        return KifuFormat::USEN;
    }
    // CSA判定（V2ヘッダまたは +/- で始まる指し手行）
    if (trimmed.startsWith(QLatin1String("V2")) ||
        trimmed.startsWith(QLatin1String("'")) ||
        csaLineStartRe.match(trimmed).hasMatch() ||
        content.contains(csaNewlineRe)) {
        qCDebug(lcKifu).noquote() << "detected format: CSA";
        return KifuFormat::CSA;
    }
    // KIF判定（"手数----" ヘッダまたは数字で始まる行）
    if (content.contains(QStringLiteral("手数----")) ||
        content.contains(kifMoveRe)) {
        qCDebug(lcKifu).noquote() << "detected format: KIF";
        return KifuFormat::KIF;
    }
    // KI2判定（▲△で始まる指し手）
    if (content.contains(ki2MoveRe)) {
        qCDebug(lcKifu).noquote() << "detected format: KI2";
        return KifuFormat::KI2;
    }
    // BOD判定（局面図のみ: 指し手を含まない局面図）
    if (trimmed.contains(QStringLiteral("後手の持駒")) ||
        trimmed.contains(QStringLiteral("先手の持駒")) ||
        trimmed.contains(bodBorderRe) ||
        trimmed.contains(QStringLiteral("|v")) ||
        trimmed.contains(QStringLiteral("| ・"))) {
        qCDebug(lcKifu).noquote() << "detected format: BOD";
        return KifuFormat::BOD;
    }

    // 不明な場合はKIFとして試す
    qCDebug(lcKifu).noquote() << "format unknown, trying KIF";
    return KifuFormat::KIF;
}

QString tempFilePath(KifuFormat fmt)
{
    QString path = QDir::tempPath() + QStringLiteral("/shogi_paste_temp");
    switch (fmt) {
    case KifuFormat::KIF:  return path + QStringLiteral(".kif");
    case KifuFormat::KI2:  return path + QStringLiteral(".ki2");
    case KifuFormat::CSA:  return path + QStringLiteral(".csa");
    case KifuFormat::USI:  return path + QStringLiteral(".usi");
    case KifuFormat::JKF:  return path + QStringLiteral(".jkf");
    case KifuFormat::USEN: return path + QStringLiteral(".usen");
    default:               return path + QStringLiteral(".kif");
    }
}

bool writeTempFile(const QString& path, const QString& content)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << content;
    out.flush();

    if (out.status() != QTextStream::Ok) {
        file.close();
        return false;
    }
    file.close();
    return true;
}

} // namespace KifuFileReader
