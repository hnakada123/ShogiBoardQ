#ifndef KIFEXPORTER_H
#define KIFEXPORTER_H

/// @file kifexporter.h
/// @brief KIF形式棋譜エクスポータクラスの定義

#include "kifuexportmetadata.h"

class KifuBranchTree;

#include <QStringList>

/**
 * @brief KIF形式の棋譜エクスポートを行うクラス
 *
 * 棋譜（分岐ツリー）と書き出しの文脈から KIF 形式の棋譜テキストを生成する。
 * 分岐の指し手にも対応している。
 */
class KifExporter
{
public:
    /**
     * @brief KIF形式の行リストを生成
     * @param recordTree 書き出す棋譜（分岐ツリー）
     * @param ctx 出力コンテキスト（ヘッダ情報等）
     * @return KIF形式の行リスト
     */
    static QStringList exportLines(const KifuBranchTree& recordTree, const KifuExportMetadata& ctx);

    /**
     * @brief SFENから開始局面のBOD行リストを生成
     * @param sfen SFEN形式の局面文字列
     * @param handicapNames true なら持駒・手番の見出しを「下手」「上手」で書く
     * @return BOD形式の行リスト（空SFENまたは平手の場合は空リスト）
     *
     * KIF / KI2 の両方で非標準局面の表示に使用される。
     */
    static QStringList sfenToBodLines(const QString& sfen, bool handicapNames = false);

    /// 先頭の行の書き方（KIF と KI2 の違い）
    struct HeaderOptions {
        bool consumedTime = true; ///< 対局情報の「消費時間」を書くか（KI2 は書かない）
    };

    /**
     * @brief KIF・KI2 の先頭（文字コード宣言・対局情報・平手でない開始局面の盤面図）を out に足す
     * @return 駒落ちの呼び方（下手・上手）で書くか（終局行・盤面図に使う）
     */
    static bool appendHeaderLines(const KifuExportMetadata& ctx, QStringList& out, HeaderOptions options);
};

#endif // KIFEXPORTER_H
