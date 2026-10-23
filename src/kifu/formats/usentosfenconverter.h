#ifndef USENTOSFENCONVERTER_H
#define USENTOSFENCONVERTER_H

/// @file usentosfenconverter.h
/// @brief USEN形式棋譜コンバータクラスの定義

#include <QString>
#include <QStringList>
#include <QList>
#include <QStringView>

#include "kifparsetypes.h"

/**
 * @brief USEN (Url Safe sfen-Extended Notation) ファイルを解析し、GUIに必要なデータを取得するクラス
 *
 * USEN仕様: https://www.slideshare.net/slideshow/scalajs-web/92707205#15
 * Shogi Playgroundで使用される形式。
 *
 * USENの主な構造:
 * - ~0. で始まるバージョン指定（0 = 平手初期局面）
 * - base36でエンコードされた指し手列（3文字 = 1手。符号表は UsenCodec）
 * - 分岐は ~(startPly). で区切られる
 * - 末尾に終局理由 (.i=反則, .r=投了, .t=時間切れ, .p=中断, .j=持将棋)
 */
class UsenToSfenConverter
{
public:
    /// 棋譜を読む。手合割の名前は平手なら「平手」、局面を書いていれば「局面指定」
    static KifuParseOutcome parse(QStringView text, const ParseOptions& options = {});

private:
    /// "~" の前に書いた開始局面（なければ平手）
    static QString initialSfenOf(const QString& content, StartKind* kind);

    // ---- USEN デコードヘルパ ----

    // USEN文字列を解析して本譜と分岐に分割
    static bool parseUsenString(const QString& usen, 
                                QString& mainlineUsen,
                                QList<QPair<int, QString>>& variations,  // (startPly, usenMoves)
                                QString* terminalCode,
                                QString* warn);

};

#endif // USENTOSFENCONVERTER_H
