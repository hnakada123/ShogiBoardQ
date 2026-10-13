#ifndef CSALEXER_H
#define CSALEXER_H

/// @file csalexer.h
/// @brief CSA形式固有の字句解析・トークン化・盤面解析

#include <QString>
#include <QStringList>

#include <optional>

#include "usimove.h"

class ShogiBoard;

namespace CsaLexer {

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

// === P行（初期局面）解析 ===

/// P1..P9 行を盤に適用
bool applyPRowLine(const QString& raw, ShogiBoard& board);

/**
 * @brief P+/P- 行を盤・駒台に適用
 * @param allRemainingTo 00AL（残りの駒すべて）を指定した側。最初の 00AL だけを覚える
 */
bool applyPPlusMinusLine(const QString& raw, ShogiBoard& board, std::optional<Turn>& allRemainingTo);

/// 00AL の後処理: 盤上・駒台にない駒（玉を除く）を side の駒台に置く
void giveRemainingPiecesTo(ShogiBoard& board, Turn side);

// === 開始局面解析 ===

/// CSA開始局面（PI/P1..P9/P+/P-/+/-）を盤に組み立て、その局面の SFEN を baseSfen に入れる（手番は盤の手番）
bool parseStartPos(const QStringList& lines, int& idx, QString& baseSfen, ShogiBoard& board);

// === 指し手解析 ===

/// CSAトークン解析結果
struct CsaMoveToken {
    BoardSquare from;   ///< 移動元（駒打ちは 0,0）
    BoardSquare to;     ///< 移動先
    Piece piece;        ///< 動かした後の駒種（先手の駒で表す）
};

/// CSAトークン（+7776FU形式）から座標と駒種を抽出（座標は0〜9の数字かだけを確かめる）
std::optional<CsaMoveToken> parseCsaMoveToken(const QString& token);

/**
 * @brief CSA指し手行を盤の上で USI/日本語表記にし、盤を進める
 *
 * 移動元の駒は指す側の駒で、指し手の駒種はその駒か成った駒でなければならない。
 * 駒打ちは指す側の駒台にある駒だけを打てる。局面と合わない指し手は盤を変えずに false を返す。
 * @param previousTo 直前の手の移動先（「同」の判定に使い、この手の移動先に更新する）
 */
bool parseMoveLine(const QString& line, Turn mover, ShogiBoard& board, BoardSquare& previousTo,
                   QString& usiMoveOut, QString& prettyOut, QString* warn);

// === コメント・時間 ===

/// CSAコメント行を正規化（'先頭を除去、**読み筋/Floodgate形式を整形）
QString normalizeCsaCommentLine(const QString& line);

/// CSA終局コード（%TORYO等）→日本語ラベル（未知コードは空文字列）
QString csaResultToLabel(const QString& token);

/// T行から消費時間（ミリ秒）を解析
std::optional<qint64> parseTimeTokenMs(const QString& token);

/// 消費時間テキスト（mm:ss/HH:MM:SS形式）を生成
QString composeTimeText(qint64 moveMs, qint64 cumMs);

} // namespace CsaLexer

#endif // CSALEXER_H
