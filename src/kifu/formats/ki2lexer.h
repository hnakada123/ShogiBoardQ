#ifndef KI2LEXER_H
#define KI2LEXER_H

/// @file ki2lexer.h
/// @brief KI2形式固有の字句解析・曖昧指し手解決

#include <QMap>
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <QVector>

#include "shogitypes.h"

namespace Ki2Lexer {

// === 候補駒構造体 ===

struct Candidate {
    int file;
    int rank;
};

// === 正規表現アクセサ ===

/// 結果行検出（キャプチャあり）: "まで○手で..." の手数をキャプチャ
const QRegularExpression& resultPatternCaptureRe();

/// 結果行検出（キャプチャなし）: "まで○手で..." の判定のみ
const QRegularExpression& resultPatternRe();

/// コロン以降を削除用: "棋戦：タイトル" → "タイトル"
const QRegularExpression& afterColonRe();

// === 行の種類判定 ===

/// KI2行が指し手行かどうか判定（▲または△で始まる）
bool isKi2MoveLine(const QString& line);

/// 結果行かどうか判定
bool isResultLine(const QString& line);

// === 結果行解析 ===

/// 結果行を解析して終局語と手数を抽出
bool parseResultLine(const QString& line, QString& terminalWord, int& moveCount);

// === 指し手テキスト解析 ===

/// 目的地（「同」対応）を読む（KI2版：▲△除去付き）
bool findDestination(const QString& moveText, int& toFile, int& toRank, bool& isSameAsPrev);

/// 駒種（漢字）→ Piece enum（基底駒の大文字）
Piece pieceKanjiToUsiUpper(const QString& s);

/// 成り表記が含まれているか（KI2版：修飾語・▲△除去付き）
bool isPromotionMoveText(const QString& moveText);

/// KI2の移動修飾語（右/左/上/引/寄/直）を抽出
QString extractMoveModifier(const QString& moveText);

/// KI2形式の1行から指し手を抽出（複数手が1行に含まれる場合あり）
QStringList extractMovesFromLine(const QString& line);

// === 駒の移動判定・候補収集 ===

/// 指定された駒が指定マスに移動できるかチェック
bool canPieceMoveTo(Piece pieceUpper, bool isPromoted,
                    int fromFile, int fromRank,
                    int toFile, int toRank,
                    bool blackToMove);

/// 移動先に到達可能な候補駒を盤面から収集
QVector<Candidate> collectCandidates(Piece pieceUpper, bool moveIsPromoted,
                                     int toFile, int toRank,
                                     bool blackToMove,
                                     const QString boardState[9][9]);

/// 右/左/上/引/寄/直 の修飾語で候補を絞り込む
QVector<Candidate> filterByDirection(const QVector<Candidate>& candidates,
                                     const QString& modifier,
                                     bool blackToMove,
                                     int toFile, int toRank);

/// 移動元座標を推測
bool inferSourceSquare(Piece pieceUpper,
                       bool moveIsPromoted,
                       int toFile, int toRank,
                       const QString& modifier,
                       bool blackToMove,
                       const QString boardState[9][9],
                       int& outFromFile, int& outFromRank);

// === KI2→USI変換 ===

/// KI2形式の指し手テキストからUSI形式に変換（盤面も更新）
QString convertKi2MoveToUsi(const QString& moveText,
                            QString boardState[9][9],
                            QMap<Piece, int>& blackHands,
                            QMap<Piece, int>& whiteHands,
                            bool blackToMove,
                            int& prevToFile, int& prevToRank);

} // namespace Ki2Lexer

#endif // KI2LEXER_H
