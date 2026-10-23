#ifndef USITOSFENCONVERTER_H
#define USITOSFENCONVERTER_H

/// @file usitosfenconverter.h
/// @brief USI形式棋譜コンバータクラスの定義

#include <QString>
#include <QStringList>
#include <QList>
#include <QStringView>

#include "kifparsetypes.h"

/**
 * @brief USI プロトコルの position コマンド文字列を解析し、GUIに必要なデータを取得するクラス
 *
 * USI仕様: https://shogidokoro2.stars.ne.jp/usi.html
 *
 * 対応フォーマット:
 * - 平手の局面を startpos で表す場合:
 *   position startpos moves 2g2f 3c3d 7g7f ...
 *
 * - 局面を SFEN 文字列で表す場合:
 *   position sfen lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1 moves 2g2f 3c3d ...
 *
 * - position が省略されている場合:
 *   startpos moves 2g2f 3c3d 7g7f ...
 *   sfen lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1 moves 2g2f 3c3d ...
 *
 * - 局面のみ（.sfen ファイルなど）。sfen 接頭辞と手数は省略可:
 *   lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1
 *
 * ファイルは最初の position/SFEN らしき行を対象にし、コメント行などは読み飛ばす。
 *
 * 終局理由コード (ShogiGUIなどで使用):
 * - resign    : 投了
 * - break     : 中断
 * - rep_draw  : 千日手
 * - draw      : 引き分け
 * - timeout   : 時間切れ
 * - win       : 入玉勝ち
 */
class UsiToSfenConverter
{
public:
    /**
     * @brief 最初の position/SFEN らしき行を読む
     *
     * 指し手も終局コードもない局面（.sfen ファイルなど）は positionOnly に局面だけを返す。
     * 開始局面の書き方は平手なら手合割（StartKind::Handicap）、それ以外は局面の値（StartKind::Specified）
     */
    static KifuParseOutcome parse(QStringView text, const ParseOptions& options = {});

private:
    // 行の中から解析対象の行（最初の position/SFEN らしき行、なければ最初の非空行）を選ぶ
    static bool selectPositionLine(const QStringList& lines, QString& content, QString* warn);

    // USI position コマンド文字列を解析して初期局面SFENと指し手列を取得
    // 入力例: "position startpos moves 2g2f 3c3d"
    // 入力例: "position sfen lnsgkgsnl/... b - 1 moves 2g2f 3c3d"
    // 入力例: "startpos moves 2g2f 3c3d" (position省略)
    // 入力例: "lnsgkgsnl/... b - 1" (局面のみ、sfen省略)
    // 開始局面を解釈できない場合は false を返す
    static bool parseUsiPositionString(const QString& usiStr,
                                       QString& baseSfen,
                                       QStringList& usiMoves,
                                       QString* terminalCode,
                                       QString* warn);
};

#endif // USITOSFENCONVERTER_H
