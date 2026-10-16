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
#include "parsecommon.h"
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

/// ファイルを読んで本譜と変化を得る
template <typename Converter>
bool parse(const QString& path, KifParseResult& out, QString* error = nullptr)
{
    out = KifParseResult{};
    QString text;
    if (!KifuLoadParser::readFileText(path, formatOf<Converter>(), text, error)) return false;
    return Converter::parseText(text, out, error);
}

/// 本譜の USI の指し手（読めなければ空）
template <typename Converter>
QStringList usiMoves(const QString& path, QString* error = nullptr)
{
    KifParseResult result;
    return parse<Converter>(path, result, error) ? result.mainline.usiMoves() : QStringList();
}

/// 本譜の表示用の指し手（読めなければ空）
template <typename Converter>
QList<KifDisplayItem> displayItems(const QString& path, QString* error = nullptr)
{
    KifParseResult result;
    return parse<Converter>(path, result, error) ? result.mainline.displayItems() : QList<KifDisplayItem>();
}

/// 開始局面（読めなければ平手）
template <typename Converter>
QString initialSfen(const QString& path, QString* detectedLabel = nullptr)
{
    QString text;
    QString warn;
    if (!KifuLoadParser::readFileText(path, formatOf<Converter>(), text, &warn)) {
        if (detectedLabel) *detectedLabel = QStringLiteral("平手(既定)");
        return SfenUtils::hirateSfen();
    }
    return Converter::detectInitialSfenFromText(text, detectedLabel);
}

/// 対局情報（USI・USEN は持たないので空）
template <typename Converter>
QList<KifGameInfoItem> gameInfo(const QString& path)
{
    if constexpr (std::is_same_v<Converter, UsiToSfenConverter> || std::is_same_v<Converter, UsenToSfenConverter>) {
        Q_UNUSED(path)
        return {};
    } else {
        QString text;
        QString warn;
        if (!KifuLoadParser::readFileText(path, formatOf<Converter>(), text, &warn)) return {};
        return Converter::extractGameInfoFromText(text);
    }
}

template <typename Converter>
QMap<QString, QString> gameInfoMap(const QString& path)
{
    return KifuParseCommon::toGameInfoMap(gameInfo<Converter>(path));
}

/// USI のファイルから開始局面・指し手・終局の語を読む
inline bool parseUsi(const QString& path, QString& baseSfen, QStringList& usiMoves, QString* terminalCode,
                     QString* warn)
{
    QString text;
    if (!KifuLoadParser::readFileText(path, KifuFormat::Usi, text, warn)) return false;
    return UsiToSfenConverter::parseUsiText(text, baseSfen, usiMoves, terminalCode, warn);
}

} // namespace KifuFile

#endif // KIFU_FILE_HELPER_H
