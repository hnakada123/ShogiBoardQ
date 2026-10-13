#ifndef CSATOSFENCONVERTER_H
#define CSATOSFENCONVERTER_H

/// @file csatosfenconverter.h
/// @brief CSA形式棋譜コンバータクラスの定義

#include "kifparsetypes.h"

#include <QString>
#include <QStringList>

/**
 * @brief CSA形式の棋譜ファイルを解析し、SFEN/USI形式およびGUI用データに変換するクラス
 *
 * CSA形式はコンピュータ将棋協会が策定した棋譜フォーマット。
 * PI/P1..P9による初期局面指定、+/-による指し手、$による対局情報を解析する。
 *
 * 字句解析・トークン化は CsaLexer 名前空間に委譲し、
 * 本クラスはファイルI/Oとパーサ状態管理のオーケストレーションを担当する。
 */
class CsaToSfenConverter
{
public:
    // --- 公開API ---

    [[nodiscard]] static bool parse(const QString& filePath, KifParseResult& out, QString* warn);
    static QList<KifGameInfoItem> extractGameInfo(const QString& filePath);

private:
    static bool readAllLinesDetectEncoding(const QString& path, QStringList& outLines, QString* warn);
};

#endif // CSATOSFENCONVERTER_H
