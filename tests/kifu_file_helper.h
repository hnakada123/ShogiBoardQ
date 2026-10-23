#ifndef KIFU_FILE_HELPER_H
#define KIFU_FILE_HELPER_H

/// @file kifu_file_helper.h
/// @brief 棋譜ファイルをアプリと同じ読み方（KifuLoadParser::readFileText）で読み、形式の読み方に渡す

#include <QMap>
#include <QStringList>

#include "csatosfenconverter.h"
#include "jkftosfenconverter.h"
#include "kiftextparser.h"
#include "kifuloadparser.h"
#include "sfenutils.h"
#include "usentosfenconverter.h"
#include "usitosfenconverter.h"

namespace KifuFile {

/// 形式の読み方（アプリの読み込み KifuLoadParser と同じ関数）
template <KifuFormat Format>
KifuParseOutcome parseText(QStringView text, const ParseOptions& options)
{
    if constexpr (Format == KifuFormat::Kif) return KifTextParser::parseKif(text, options);
    else if constexpr (Format == KifuFormat::Ki2) return KifTextParser::parseKi2(text, options);
    else if constexpr (Format == KifuFormat::Csa) return CsaToSfenConverter::parse(text, options);
    else if constexpr (Format == KifuFormat::Jkf) return JkfToSfenConverter::parse(text, options);
    else if constexpr (Format == KifuFormat::Usen) return UsenToSfenConverter::parse(text, options);
    else return UsiToSfenConverter::parse(text, options);
}

/// ファイルを読んで形式の parse() に渡す（ファイルが読めなければ失敗の結果）
template <KifuFormat Format>
KifuParseOutcome parseFile(const QString& path, const ParseOptions& options = {})
{
    QString text;
    QString warn;
    if (!KifuLoadParser::readFileText(path, Format, text, &warn)) {
        KifuParseOutcome failed;
        failed.warning = warn;
        return failed;
    }
    return parseText<Format>(text, options);
}

/// ファイルを読んで本譜と変化を得る（警告は error に足す）
template <KifuFormat Format>
bool parse(const QString& path, KifParseResult& out, QString* error = nullptr)
{
    KifuParseOutcome outcome = parseFile<Format>(path);
    if (error) *error += outcome.warning;
    out = outcome.success ? std::move(outcome.record) : KifParseResult{};
    return outcome.success;
}

/// 本譜の USI の指し手（読めなければ空）
template <KifuFormat Format>
QStringList usiMoves(const QString& path, QString* error = nullptr)
{
    KifParseResult result;
    return parse<Format>(path, result, error) ? result.mainline.usiMoves() : QStringList();
}

/// 本譜（読めなければ空の手順）
template <KifuFormat Format>
RecordLine mainline(const QString& path, QString* error = nullptr)
{
    KifParseResult result;
    return parse<Format>(path, result, error) ? result.mainline : RecordLine{};
}

/// 開始局面（読めなければ平手）。kind には開始局面の書き方を入れる（読めなければ Default）
template <KifuFormat Format>
QString initialSfen(const QString& path, StartKind* kind = nullptr)
{
    const KifuParseOutcome outcome = parseFile<Format>(path);
    if (!outcome.success || outcome.record.mainline.startSfen.isEmpty()) {
        if (kind) *kind = StartKind::Default;
        return SfenUtils::hirateSfen();
    }
    if (kind) *kind = outcome.startKind;
    return outcome.record.mainline.startSfen;
}

/// 対局情報（USI・USEN は持たないので空）
template <KifuFormat Format>
QList<KifGameInfoItem> gameInfo(const QString& path)
{
    return parseFile<Format>(path).gameInfo;
}

/// 対局情報をキーで引ける形にする（同じキーは後のもの）
inline QMap<QString, QString> toMap(const QList<KifGameInfoItem>& items)
{
    QMap<QString, QString> map;
    for (const auto& item : items) map.insert(item.key, item.value);
    return map;
}

template <KifuFormat Format>
QMap<QString, QString> gameInfoMap(const QString& path)
{
    return toMap(gameInfo<Format>(path));
}

/// USI のファイルから開始局面・指し手・終局の有無を読む（terminalCode は終局があれば終局語）
inline bool parseUsi(const QString& path, QString& baseSfen, QStringList& usiMoves, QString* terminalCode,
                     QString* warn)
{
    const KifuParseOutcome outcome = parseFile<KifuFormat::Usi>(path);
    if (warn) *warn += outcome.warning;
    if (!outcome.success) return false;
    baseSfen = outcome.record.mainline.startSfen;
    usiMoves = outcome.record.mainline.usiMoves();
    if (terminalCode) {
        const RecordEntry* terminal = outcome.record.mainline.terminal();
        *terminalCode = terminal ? TerminalTypes::label(terminal->terminal) : QString();
    }
    return true;
}

} // namespace KifuFile

#endif // KIFU_FILE_HELPER_H
