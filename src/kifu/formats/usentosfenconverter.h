#ifndef USENTOSFENCONVERTER_H
#define USENTOSFENCONVERTER_H

/// @file usentosfenconverter.h
/// @brief USEN形式棋譜コンバータクラスの定義

#include <QString>
#include <QStringList>
#include <QList>
#include <QMap>

#include "kifdisplayitem.h"
#include "kifparsetypes.h"

/// @brief USEN デコード結果（エラー情報付き）
struct UsenDecodeResult {
    QStringList moves;       ///< デコードされた USI 手順
    int invalidCount = 0;    ///< デコード失敗した手数
    QString firstError;      ///< 最初のエラー内容（失敗がなければ空）
};

/**
 * @brief USEN (Url Safe sfen-Extended Notation) ファイルを解析し、GUIに必要なデータを取得するクラス
 *
 * USEN仕様: https://www.slideshare.net/slideshow/scalajs-web/92707205#15
 * Shogi Playgroundで使用される形式。
 *
 * USENの主な構造:
 * - ~0. で始まるバージョン指定（0 = 平手初期局面）
 * - base36でエンコードされた指し手列（3文字 = 1手）
 * - 分岐は ~(startPly). で区切られる
 * - 末尾に終局理由 (.i=反則, .r=投了, .t=時間切れ, .p=中断, .j=持将棋)
 */
class UsenToSfenConverter
{
public:

    // テキスト（貼り付けた棋譜など）版
    static QString detectInitialSfenFromText(const QString& text, QString* detectedLabel = nullptr);
    [[nodiscard]] static bool parseText(const QString& text, KifParseResult& out, QString* errorMessage = nullptr);

    // USEN文字列から指し手列をデコード
    static QStringList decodeUsenMoves(const QString& usenStr, QString* terminalOut = nullptr);

    // USEN文字列から指し手列をデコード（エラー情報付き厳密版）
    static UsenDecodeResult decodeUsenMovesStrict(const QString& usenStr, QString* terminalOut = nullptr);

private:
    // ---- USEN デコードヘルパ ----

    // USEN文字列を解析して本譜と分岐に分割
    static bool parseUsenString(const QString& usen, 
                                QString& mainlineUsen,
                                QList<QPair<int, QString>>& variations,  // (startPly, usenMoves)
                                QString* terminalCode,
                                QString* warn);

    // base36でエンコードされた指し手をUSI形式に変換
    static QString decodeBase36Move(const QString& base36str);

    // base36文字から整数へ変換
    static int base36CharToInt(QChar c);

    // 3文字のbase36から指し手番号（0-46655の範囲）を取得
    static int base36ToMoveIndex(const QString& threeChars);

};

#endif // USENTOSFENCONVERTER_H
