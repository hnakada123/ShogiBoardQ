#ifndef KIFU_FILE_HELPER_H
#define KIFU_FILE_HELPER_H

/// @file kifu_file_helper.h
/// @brief 棋譜ファイルをアプリと同じ読み方（KifuLoadParser::readFileText）で読み、各形式の変換器に渡す

#include <QMap>
#include <QStringList>
#include <type_traits>

#include "csatosfenconverter.h"
#include "jkftosfenconverter.h"
#include "ki2tosfenconverter.h"
#include "kiftosfenconverter.h"
#include "kifuloadparser.h"
#include "sfenutils.h"
#include "usentosfenconverter.h"
#include "usitosfenconverter.h"

namespace KifuFile {

template <typename Converter>
constexpr KifuFormat formatOf()
{
    if constexpr (std::is_same_v<Converter, KifToSfenConverter>) return KifuFormat::Kif;
    else if constexpr (std::is_same_v<Converter, Ki2ToSfenConverter>) return KifuFormat::Ki2;
    else if constexpr (std::is_same_v<Converter, CsaToSfenConverter>) return KifuFormat::Csa;
    else if constexpr (std::is_same_v<Converter, JkfToSfenConverter>) return KifuFormat::Jkf;
    else if constexpr (std::is_same_v<Converter, UsenToSfenConverter>) return KifuFormat::Usen;
    else return KifuFormat::Usi;
}

/// ファイルを読んで形式の parse() に渡す（ファイルが読めなければ失敗の結果）
template <typename Converter>
KifuParseOutcome parseFile(const QString& path, const ParseOptions& options = {})
{
    QString text;
    QString warn;
    if (!KifuLoadParser::readFileText(path, formatOf<Converter>(), text, &warn)) {
        KifuParseOutcome failed;
        failed.warning = warn;
        return failed;
    }
    return Converter::parse(text, options);
}

/// ファイルを読んで本譜と変化を得る（警告は error に足す）
template <typename Converter>
bool parse(const QString& path, KifParseResult& out, QString* error = nullptr)
{
    KifuParseOutcome outcome = parseFile<Converter>(path);
    if (error) *error += outcome.warning;
    out = outcome.success ? std::move(outcome.record) : KifParseResult{};
    return outcome.success;
}

/// 本譜の USI の指し手（読めなければ空）
template <typename Converter>
QStringList usiMoves(const QString& path, QString* error = nullptr)
{
    KifParseResult result;
    return parse<Converter>(path, result, error) ? result.mainline.usiMoves() : QStringList();
}

/// 本譜（読めなければ空の手順）
template <typename Converter>
RecordLine mainline(const QString& path, QString* error = nullptr)
{
    KifParseResult result;
    return parse<Converter>(path, result, error) ? result.mainline : RecordLine{};
}

/// 開始局面（読めなければ平手）
template <typename Converter>
QString initialSfen(const QString& path, QString* detectedLabel = nullptr)
{
    const KifuParseOutcome outcome = parseFile<Converter>(path);
    if (!outcome.success || outcome.initialSfen.isEmpty()) {
        if (detectedLabel) *detectedLabel = QStringLiteral("平手(既定)");
        return SfenUtils::hirateSfen();
    }
    if (detectedLabel) *detectedLabel = outcome.teaiLabel;
    return outcome.initialSfen;
}

/// 対局情報（USI・USEN は持たないので空）
template <typename Converter>
QList<KifGameInfoItem> gameInfo(const QString& path)
{
    return parseFile<Converter>(path).gameInfo;
}

/// 対局情報をキーで引ける形にする（同じキーは後のもの）
inline QMap<QString, QString> toMap(const QList<KifGameInfoItem>& items)
{
    QMap<QString, QString> map;
    for (const auto& item : items) map.insert(item.key, item.value);
    return map;
}

template <typename Converter>
QMap<QString, QString> gameInfoMap(const QString& path)
{
    return toMap(gameInfo<Converter>(path));
}

/// USI のファイルから開始局面・指し手・終局の有無を読む（terminalCode は終局があれば終局語）
inline bool parseUsi(const QString& path, QString& baseSfen, QStringList& usiMoves, QString* terminalCode,
                     QString* warn)
{
    const KifuParseOutcome outcome = parseFile<UsiToSfenConverter>(path);
    if (warn) *warn += outcome.warning;
    if (!outcome.success) return false;
    baseSfen = outcome.initialSfen;
    usiMoves = outcome.record.mainline.usiMoves();
    if (terminalCode) {
        const RecordEntry* terminal = outcome.record.mainline.terminal();
        *terminalCode = terminal ? TerminalTypes::label(terminal->terminal) : QString();
    }
    return true;
}

} // namespace KifuFile

#endif // KIFU_FILE_HELPER_H
