#ifndef CSALEXER_H
#define CSALEXER_H

/// @file csalexer.h
/// @brief CSA形式固有の字句解析・トークン化・盤面解析

#include <QString>
#include <QStringList>

namespace CsaLexer {

// === 手番・駒種の型定義 ===

/// 手番
enum Color { Black, White, None };

/// 駒種
enum Piece {
    NO_P, FU, KY, KE, GI, KI, KA, HI, OU,
    TO, NY, NK, NG, UM, RY
};

/// 盤上の1マス
struct Cell {
    Piece p = NO_P;
    Color c = None;
};

/// 9x9盤面
struct Board {
    Cell sq[10][10]; ///< [x][y], インデックス1..9が有効範囲
};

// === 盤面初期化 ===

/// 平手初期配置に設定する
void setHirate(Board& board);

// === 行の種類判定 ===

/// 指し手行（+/-で始まる2文字以上の行）
bool isMoveLine(const QString& s);

/// 結果行（%で始まる行）
bool isResultLine(const QString& s);

/// メタ行（V/$/ Nで始まる行）
bool isMetaLine(const QString& s);

/// コメント行（'で始まる行）
bool isCommentLine(const QString& s);

/// 手番マーカー（"+"または"-"の1文字のみ）
bool isTurnMarker(const QString& token);

// === 駒種変換 ===

/// CSA 2文字駒コード（"FU","KY"等）→ Piece enum
Piece pieceFromCsa2(const QString& two);

/// 成駒かどうか判定
bool isPromotedPiece(Piece p);

/// 成駒→基底駒変換
Piece basePieceOf(Piece p);

/// Piece → 漢字表記
QString pieceKanji(Piece p);

/// 座標範囲チェック（1..9）
bool inside(int v);

// === P行（初期局面）解析 ===

/// P1..P9 行を盤面に適用
bool applyPRowLine(const QString& raw, Board& b);

/// P+/P- 行を盤面・持駒に適用
bool applyPPlusMinusLine(const QString& raw, Board& b,
                          int bH[7], int wH[7], bool& alBlack, bool& alWhite);

/// 00AL後処理: 未使用駒を指定側の持駒に追加
void processAlRemainder(Board& board, int bH[7], int wH[7], bool alBlack, bool alWhite);

// === 盤面→SFEN変換 ===

/// 盤面→SFEN（盤面フィールドのみ）
QString toSfenBoard(const Board& b);

/// 持駒→SFEN
QString handsToSfen(const int bH[7], const int wH[7]);

// === 開始局面解析 ===

/// CSA開始局面（PI/P1..P9/P+/P-/+/-）を解析
bool parseStartPos(const QStringList& lines, int& idx,
                   QString& baseSfen, Color& stm, Board& board);

// === 指し手解析 ===

/// CSAトークン（+7776FU形式）から座標と駒種を抽出
bool parseCsaMoveToken(const QString& token,
                       int& fx, int& fy, int& tx, int& ty, Piece& afterPiece);

/// CSA指し手行をUSI/日本語表記に変換し盤面を更新
bool parseMoveLine(const QString& line, Color mover, Board& b,
                   int& prevTx, int& prevTy,
                   QString& usiMoveOut, QString& prettyOut, QString* warn);

// === コメント・時間 ===

/// CSAコメント行を正規化（'先頭を除去、**読み筋/Floodgate形式を整形）
QString normalizeCsaCommentLine(const QString& line);

/// CSA終局コード（%TORYO等）→日本語ラベル（未知コードは空文字列）
QString csaResultToLabel(const QString& token);

/// T行から消費時間（ミリ秒）を解析
bool parseTimeTokenMs(const QString& token, qint64& msOut);

/// 消費時間テキスト（mm:ss/HH:MM:SS形式）を生成
QString composeTimeText(qint64 moveMs, qint64 cumMs);

} // namespace CsaLexer

#endif // CSALEXER_H
