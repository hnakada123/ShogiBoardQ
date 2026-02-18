#ifndef KI2TOSFENCONVERTER_H
#define KI2TOSFENCONVERTER_H

/// @file ki2tosfenconverter.h
/// @brief KI2形式棋譜コンバータクラスの定義


#include <QString>
#include <QStringList>
#include <QList>
#include <QVector>
#include <QPair>
#include <QMap>
#include <QPoint>
#include "shogimove.h"
#include "kifdisplayitem.h"
#include "kifparsetypes.h"

/**
 * @brief KI2形式の棋譜を解析し、USI形式やGUI用データに変換するクラス
 *
 * KI2形式はKIF形式と異なり、移動元座標が省略されているため、
 * 盤面状態を追跡しながら移動元を推測する必要がある。
 *
 * KI2形式の特徴:
 * - 指し手は「▲２六歩」「△８四歩」のように手番記号で始まる
 * - 移動元座標がない（KIFでは「(27)」のように記載される）
 * - 同一駒が複数ある場合は「右」「左」「上」「引」「寄」「直」などの修飾語で区別
 * - 複数の指し手が1行に書かれることがある
 */
class Ki2ToSfenConverter
{
public:
    // 既存API互換：本譜の初期SFEN（手合割から判定）
    static QString detectInitialSfenFromFile(const QString& ki2Path, QString* detectedLabel = nullptr);

    // 既存API互換：本譜のUSI列のみを抽出（終局/中断で打ち切り）
    static QStringList convertFile(const QString& ki2Path, QString* errorMessage = nullptr);

    // 既存API互換：本譜の「指し手＋時間」（コメントも格納）を抽出
    static QList<KifDisplayItem> extractMovesWithTimes(const QString& ki2Path, QString* errorMessage = nullptr);

    // 新API：本譜＋全変化をまとめて抽出（コメントも格納）
    static bool parseWithVariations(const QString& ki2Path, KifParseResult& out, QString* errorMessage = nullptr);

    // 手合→初期SFEN（ユーザー提供のマップ）
    static QString mapHandicapToSfen(const QString& label);

    // 追加: KI2ファイルから「対局情報」を抽出して順序付きで返す
    static QList<KifGameInfoItem> extractGameInfo(const QString& filePath);

    // 追加（任意）：キー→値 へ高速アクセスしたい場合のマップ版
    static QMap<QString, QString> extractGameInfoMap(const QString& filePath);

    // BOD形式の盤面からSFENを構築
    static bool buildInitialSfenFromBod(const QStringList& lines, QString& outSfen,
                                        QString* detectedLabel = nullptr, QString* warn = nullptr);

private:
    // ---------- KI2固有のヘルパ ----------

    // KI2形式の1行から指し手を抽出（複数手が1行に含まれる場合あり）
    // returns: 抽出した指し手のリスト（▲/△付き）
    static QStringList extractMovesFromLine(const QString& line);

    // KI2形式の指し手テキストからUSI形式に変換
    // boardState: 現在の盤面状態（9x9配列、空マスは空文字列）
    // hands: 持ち駒（先手/後手それぞれの駒種別枚数）
    // blackToMove: 先手番ならtrue
    // prevToFile, prevToRank: 直前の移動先座標（「同」対応）
    // 戻り値: USI形式の指し手（失敗時は空文字列）
    static QString convertKi2MoveToUsi(const QString& moveText,
                                        QString boardState[9][9],
                                        QMap<QChar, int>& blackHands,
                                        QMap<QChar, int>& whiteHands,
                                        bool blackToMove,
                                        int& prevToFile, int& prevToRank);

    // 盤面に指し手を適用
    static void applyMoveToBoard(const QString& usi,
                                  QString boardState[9][9],
                                  QMap<QChar, int>& blackHands,
                                  QMap<QChar, int>& whiteHands,
                                  bool blackToMove);

    // SFENから盤面状態を初期化
    static void initBoardFromSfen(const QString& sfen,
                                   QString boardState[9][9],
                                   QMap<QChar, int>& blackHands,
                                   QMap<QChar, int>& whiteHands);

    // 移動元座標を推測
    static bool inferSourceSquare(QChar pieceUpper,
                                   bool moveIsPromoted,
                                   int toFile, int toRank,
                                   const QString& modifier,
                                   bool blackToMove,
                                   const QString boardState[9][9],
                                   int& outFromFile, int& outFromRank);

    // inferSourceSquare のヘルパ
    struct Candidate {
        int file;
        int rank;
    };

    // 移動先に到達可能な候補駒を盤面から収集
    static QVector<Candidate> collectCandidates(QChar pieceUpper, bool moveIsPromoted,
                                                 int toFile, int toRank,
                                                 bool blackToMove,
                                                 const QString boardState[9][9]);

    // 右/左/上/引/寄/直 の修飾語で候補を絞り込む
    static QVector<Candidate> filterByDirection(const QVector<Candidate>& candidates,
                                                 const QString& modifier,
                                                 bool blackToMove,
                                                 int toFile, int toRank);

    // 指定された駒が指定マスに移動できるかチェック
    static bool canPieceMoveTo(QChar pieceUpper, bool isPromoted,
                                int fromFile, int fromRank,
                                int toFile, int toRank,
                                bool blackToMove);

    // 盤面トークンから駒情報を抽出
    // returns: (基本駒種の大文字, 成りフラグ, 先手フラグ)
    static bool parseToken(const QString& token, QChar& pieceUpper, bool& isPromoted, bool& isBlack);

    // ---------- 共通ヘルパ（KifToSfenConverterと同様） ----------
    static bool isSkippableLine(const QString& line);
    static bool isBoardHeaderOrFrame(const QString& line);
    static bool isBodHeader(const QString& line);
    static bool containsAnyTerminal(const QString& s, QString* matched = nullptr);
    static int  kanjiDigitToInt(QChar c);
    static QChar rankNumToLetter(int r); // 1..9 -> 'a'..'i'

    // 目的地（「同」対応）を読む
    static bool findDestination(const QString& moveText, int& toFile, int& toRank, bool& isSameAsPrev);

    // 駒種（漢字）→ USI基底文字（'P','L',...）
    static QChar pieceKanjiToUsiUpper(const QString& s);

    // 成駒表記が含まれているか
    static bool isPromotionMoveText(const QString& moveText);

    // KI2の移動修飾語を抽出
    static QString extractMoveModifier(const QString& moveText);

    // 追加API：初手前（開始局面）のコメント／しおりをまとめて取得
    static QString extractOpeningComment(const QString& filePath);

    // KI2行が指し手行かどうか判定（▲または△で始まる）
    static bool isKi2MoveLine(const QString& line);

    // コメント行判定
    static bool isCommentLine(const QString& s);

    // しおり行判定
    static bool isBookmarkLine(const QString& s);
};

#endif // KI2TOSFENCONVERTER_H
