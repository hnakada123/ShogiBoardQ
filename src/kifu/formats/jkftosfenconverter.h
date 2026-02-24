#ifndef JKFTOSFENCONVERTER_H
#define JKFTOSFENCONVERTER_H

/// @file jkftosfenconverter.h
/// @brief JKF形式棋譜コンバータクラスの定義


#include <QString>
#include <QStringList>
#include <QList>
#include <QVector>
#include <QMap>
#include <QJsonObject>
#include <QJsonArray>

#include "kifdisplayitem.h"
#include "kifparsetypes.h"

/**
 * @brief JSON棋譜フォーマット(JKF)ファイルを解析し、GUIに必要なデータを取得するクラス
 *
 * JKF仕様: https://github.com/na2hiro/json-kifu-format
 *
 * JKFの主な構造:
 * - header: 対局情報（先手, 後手, 棋戦など）
 * - initial: 初期局面設定（preset または data）
 * - moves: 指し手配列（コメント、分岐情報を含む）
 */
class JkfToSfenConverter
{
public:
    // 既存API互換: 初期局面SFENを取得（手合割から判定）
    static QString detectInitialSfenFromFile(const QString& jkfPath, QString* detectedLabel = nullptr);

    // 既存API互換: 本譜のUSI列のみを抽出（終局で打ち切り）
    static QStringList convertFile(const QString& jkfPath, QString* errorMessage = nullptr);

    // 既存API互換: 本譜の「指し手＋時間」（コメントも格納）を抽出
    static QList<KifDisplayItem> extractMovesWithTimes(const QString& jkfPath, QString* errorMessage = nullptr);

    // 新API: 本譜＋全変化をまとめて抽出（コメントも格納）
    static bool parseWithVariations(const QString& jkfPath, KifParseResult& out, QString* errorMessage = nullptr);

    // JKFファイルから「対局情報」を抽出して順序付きで返す
    static QList<KifGameInfoItem> extractGameInfo(const QString& filePath);

    // キー→値 へ高速アクセスしたい場合のマップ版
    static QMap<QString, QString> extractGameInfoMap(const QString& filePath);

    // preset名 → 初期SFEN へ変換
    static QString mapPresetToSfen(const QString& preset);

private:
    // ---- JKF JSON解析ヘルパ ----

    // JSONファイルを読み込みルートオブジェクトを返す
    static bool loadJsonFile(const QString& filePath, QJsonObject& root, QString* warn);

    // initial フィールドから初期局面SFENを構築
    static QString buildInitialSfen(const QJsonObject& root, QString* detectedLabel = nullptr);

    // initial.data から局面SFENを構築（カスタム初期局面用）
    static QString buildSfenFromInitialData(const QJsonObject& data, int moveNumber = 1);

    // moves 配列を解析して本譜データを生成
    static void parseMovesArray(const QJsonArray& movesArray,
                                const QString& baseSfen,
                                KifLine& mainline,
                                QVector<KifVariation>& variations,
                                QString* warn);

    // move オブジェクトからUSI形式指し手文字列を生成
    static QString convertMoveToUsi(const QJsonObject& move, int& prevToX, int& prevToY);

    // move オブジェクトから日本語表記（▲△付き）を生成
    static QString convertMoveToPretty(const QJsonObject& move, int plyNumber, int& prevToX, int& prevToY);

    // time オブジェクトから時間文字列を生成（mm:ss/HH:MM:SS形式）
    static QString formatTimeText(const QJsonObject& timeObj, qint64& cumSec);

    // special 文字列から終局語を日本語に変換
    static QString specialToJapanese(const QString& special);

    // 盤面を進める（検証用）
    struct Board {
        int cells[10][10];  // cells[x][y], 0=空, 正=先手, 負=後手
        int hands[2][8];    // hands[color][pieceType], 0=先手, 1=後手
        int turn;           // 0=先手, 1=後手

        void setHirate();
        void clear();
    };

    // CSA形式の駒種文字列をintに変換（内部用）
    static int pieceKindFromCsa(const QString& kind);

    // 駒種の日本語名を取得
    static QString pieceKindToKanji(const QString& kind);

    // 相対情報（relative）から修飾語を生成（右、左、上、引など）
    static QString relativeToModifier(const QString& relative);
};

#endif // JKFTOSFENCONVERTER_H
