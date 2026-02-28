#ifndef USITOSFENCONVERTER_H
#define USITOSFENCONVERTER_H

/// @file usitosfenconverter.h
/// @brief USI形式棋譜コンバータクラスの定義


#include <QString>
#include <QStringList>
#include <QList>
#include <QVector>
#include <QMap>

#include "kifdisplayitem.h"
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
    // 既存API互換: 初期局面SFENを取得
    static QString detectInitialSfenFromFile(const QString& usiPath, QString* detectedLabel = nullptr);

    // 既存API互換: 本譜のUSI列のみを抽出（終局で打ち切り）
    static QStringList convertFile(const QString& usiPath, QString* errorMessage = nullptr);

    // 既存API互換: 本譜の「指し手＋時間」（コメントも格納）を抽出
    static QList<KifDisplayItem> extractMovesWithTimes(const QString& usiPath, QString* errorMessage = nullptr);

    // 新API: 本譜＋全変化をまとめて抽出（コメントも格納）
    [[nodiscard]] static bool parseWithVariations(const QString& usiPath, KifParseResult& out, QString* errorMessage = nullptr);

    // USIファイルから「対局情報」を抽出して順序付きで返す
    // 注意: USIは基本的にメタ情報を含まないため、ファイル名から推測するか空を返す
    static QList<KifGameInfoItem> extractGameInfo(const QString& filePath);

    // キー→値 へ高速アクセスしたい場合のマップ版
    static QMap<QString, QString> extractGameInfoMap(const QString& filePath);

    // 終局理由コードから日本語表記に変換
    static QString terminalCodeToJapanese(const QString& code);

private:
    // ---- USI ファイル読み込み ----

    // USIファイルを読み込み文字列を返す
    static bool readUsiFile(const QString& filePath, QString& content, QString* warn);

    // ---- USI 文字列解析 ----

    // USI position コマンド文字列を解析して初期局面SFENと指し手列を取得
    // 入力例: "position startpos moves 2g2f 3c3d"
    // 入力例: "position sfen lnsgkgsnl/... b - 1 moves 2g2f 3c3d"
    // 入力例: "startpos moves 2g2f 3c3d" (position省略)
    static bool parseUsiPositionString(const QString& usiStr,
                                       QString& baseSfen,
                                       QStringList& usiMoves,
                                       QString* terminalCode,
                                       QString* warn);

    // 本譜/変化のデータを構築（共通パイプライン使用版）
    static void buildKifLine(const QStringList& usiMoves,
                             const QString& baseSfen,
                             int startPly,
                             const QString& terminalCode,
                             KifLine& outLine,
                             QString* warn);
};

#endif // USITOSFENCONVERTER_H
