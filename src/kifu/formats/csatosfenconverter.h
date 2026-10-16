#ifndef CSATOSFENCONVERTER_H
#define CSATOSFENCONVERTER_H

/// @file csatosfenconverter.h
/// @brief CSA形式棋譜コンバータクラスの定義

#include <QString>
#include <QStringList>
#include <QStringView>

#include "kifparsetypes.h"

/**
 * @brief CSA形式の棋譜を読む
 *
 * CSA形式はコンピュータ将棋協会が策定した棋譜フォーマット。
 * PI/P1..P9による初期局面指定、+/-による指し手、$による対局情報を解析する。
 * 字句解析は CsaLexer、手順の組み立ては KifLineBuilder に任せる。
 */
class CsaToSfenConverter
{
public:
    /// 棋譜を読む。手合割の名前は平手なら「平手」、それ以外は「局面指定」
    static KifuParseOutcome parse(QStringView text, const ParseOptions& options = {});

    /// CSA ファイルをテキストとして読む（"CSA encoding=" の宣言、なければ UTF-8 → Shift_JIS の順に判別）
    static bool readFileText(const QString& path, QString& outText, QString* warn);

private:
    /// テキストを1文ずつの行にする（カンマ区切りのマルチステートメントを展開し、2局目以降は読まない）
    static QStringList splitStatements(const QString& text, QString* warn);
};

#endif // CSATOSFENCONVERTER_H
