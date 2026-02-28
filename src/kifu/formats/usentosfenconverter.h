#ifndef USENTOSFENCONVERTER_H
#define USENTOSFENCONVERTER_H

/// @file usentosfenconverter.h
/// @brief USEN形式棋譜コンバータクラスの定義


#include <QString>
#include <QStringList>
#include <QList>
#include <QVector>
#include <QMap>

#include "kifdisplayitem.h"
#include "kifparsetypes.h"

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
    // 既存API互換: 初期局面SFENを取得
    static QString detectInitialSfenFromFile(const QString& usenPath, QString* detectedLabel = nullptr);

    // 既存API互換: 本譜のUSI列のみを抽出（終局で打ち切り）
    static QStringList convertFile(const QString& usenPath, QString* errorMessage = nullptr);

    // 既存API互換: 本譜の「指し手＋時間」（コメントも格納）を抽出
    static QList<KifDisplayItem> extractMovesWithTimes(const QString& usenPath, QString* errorMessage = nullptr);

    // 新API: 本譜＋全変化をまとめて抽出（コメントも格納）
    [[nodiscard]] static bool parseWithVariations(const QString& usenPath, KifParseResult& out, QString* errorMessage = nullptr);

    // USENファイルから「対局情報」を抽出して順序付きで返す
    // 注意: USENは基本的にメタ情報を含まないため、ファイル名から推測するか空を返す
    static QList<KifGameInfoItem> extractGameInfo(const QString& filePath);

    // キー→値 へ高速アクセスしたい場合のマップ版
    static QMap<QString, QString> extractGameInfoMap(const QString& filePath);

    // USEN文字列から指し手列をデコード
    static QStringList decodeUsenMoves(const QString& usenStr, QString* terminalOut = nullptr);

    // 終局理由コードから日本語表記に変換
    static QString terminalCodeToJapanese(const QString& code);

private:
    // ---- USEN デコードヘルパ ----

    // USENファイルを読み込み文字列を返す
    static bool readUsenFile(const QString& filePath, QString& content, QString* warn);

    // USEN文字列を解析して本譜と分岐に分割
    static bool parseUsenString(const QString& usen, 
                                QString& mainlineUsen,
                                QVector<QPair<int, QString>>& variations,  // (startPly, usenMoves)
                                QString* terminalCode,
                                QString* warn);

    // base36でエンコードされた指し手をUSI形式に変換
    static QString decodeBase36Move(const QString& base36str);

    // base36文字から整数へ変換
    static int base36CharToInt(QChar c);

    // 3文字のbase36から指し手番号（0-46655の範囲）を取得
    static int base36ToMoveIndex(const QString& threeChars);

    // 本譜/変化のデータを構築（共通パイプライン使用版）
    static void buildKifLine(const QStringList& usiMoves,
                             const QString& baseSfen,
                             int startPly,
                             const QString& terminalCode,
                             KifLine& outLine,
                             QString* warn);
};

#endif // USENTOSFENCONVERTER_H
