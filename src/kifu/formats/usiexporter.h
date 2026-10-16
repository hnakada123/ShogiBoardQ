#ifndef USIEXPORTER_H
#define USIEXPORTER_H

/// @file usiexporter.h
/// @brief USI形式棋譜エクスポータクラスの定義

#include "kifuexportmetadata.h"

class KifuBranchTree;

#include <QStringList>

/**
 * @brief USIプロトコル形式（position コマンド文字列）の棋譜エクスポートを行うクラス
 *
 * 棋譜（分岐ツリー）と書き出しの文脈から USI position コマンド形式の棋譜テキストを生成する。
 * 分岐の指し手には対応していない（本譜のみ出力）。
 * https://shogidokoro2.stars.ne.jp/usi.html
 */
class UsiExporter
{
public:
    /**
     * @brief USI形式の行リストを生成
     * @param recordTree 書き出す棋譜（分岐ツリー）
     * @param ctx 出力コンテキスト（ヘッダ情報等）
     * @return USI形式の行リスト（1要素のposition コマンド文字列）
     */
    static QStringList exportLines(const KifuBranchTree& recordTree, const KifuExportMetadata& ctx);

    /// 指定された経路だけを出力する。終局表示はその経路に含まれる場合だけ渡す。
    static QStringList exportPosition(const QString& startSfen, const QStringList& usiMoves,
                                      TerminalType terminal = TerminalType::None);
};

#endif // USIEXPORTER_H
