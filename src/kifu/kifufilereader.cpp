/// @file kifufilereader.cpp
/// @brief 棋譜ファイル読み込みI/O層の実装

#include "kifufilereader.h"
#include "kifreader.h"
#include "logcategories.h"

#include <QFileInfo>

#include <QDir>
#include <QRegularExpression>

namespace KifuFileReader {

KifuFormat detectFileFormat(const QString& filePath)
{
    const QString suffix = QFileInfo(filePath).suffix().toLower();
    if (suffix == QLatin1String("kif") || suffix == QLatin1String("kifu")) return KifuFormat::KIF;
    if (suffix == QLatin1String("ki2") || suffix == QLatin1String("ki2u")) return KifuFormat::KI2;
    if (suffix == QLatin1String("csa")) return KifuFormat::CSA;
    if (suffix == QLatin1String("jkf")) return KifuFormat::JKF;
    if (suffix == QLatin1String("usen")) return KifuFormat::USEN;
    if (suffix == QLatin1String("usi") || suffix == QLatin1String("sfen")) return KifuFormat::USI;

    // JKF は .json で配布されることも多い。拡張子で決まらないときは内容で判定する。
    QStringList lines;
    QString usedEncoding;
    if (KifReader::readLinesAuto(filePath, lines, &usedEncoding, nullptr)) {
        const KifuFormat detected = detectFormat(lines.join(QLatin1Char('\n')));
        switch (detected) {
        case KifuFormat::KI2:
        case KifuFormat::CSA:
        case KifuFormat::JKF:
        case KifuFormat::USEN:
        case KifuFormat::USI:
            return detected;
        case KifuFormat::SFEN:
            return KifuFormat::USI;
        default:
            break;
        }
    }
    return KifuFormat::KIF;
}

KifuFormat detectFormat(const QString& content)
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
        return KifuFormat::SFEN;
    }
    // JSON判定（JKF）
    if (trimmed.startsWith(QLatin1Char('{')) || trimmed.startsWith(QLatin1Char('['))) {
        qCDebug(lcKifu).noquote() << "detected format: JKF (JSON)";
        return KifuFormat::JKF;
    }
    // USI判定
    if (usiLineRe.match(trimmed).hasMatch()) {
        qCDebug(lcKifu).noquote() << "detected format: USI";
        return KifuFormat::USI;
    }
    // USEN全体の構造を確認し、コメントやURL内のチルダを除外する。
    if (usenRe.match(trimmed).hasMatch()) {
        qCDebug(lcKifu).noquote() << "detected format: USEN";
        return KifuFormat::USEN;
    }
    // 指し手がないV3・旧版の局面ファイルも判定する。
    if (csaHeaderRe.match(trimmed).hasMatch() ||
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
        trimmed.contains(QStringLiteral("上手の持駒")) ||
        trimmed.contains(QStringLiteral("下手の持駒")) ||
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

static QString tempFileTemplate(KifuFormat fmt)
{
    QString path = QDir::tempPath() + QStringLiteral("/shogi_paste_XXXXXX");
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

std::unique_ptr<QTemporaryFile> createTempFile(KifuFormat fmt, const QString& content)
{
    auto file = std::make_unique<QTemporaryFile>(tempFileTemplate(fmt));
    if (!file->open()) return nullptr;
    const QByteArray data = content.toUtf8();
    if (file->write(data) != data.size() || !file->flush()) return nullptr;
    file->close();
    return file;
}

} // namespace KifuFileReader
