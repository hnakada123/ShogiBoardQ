#ifndef KIFUEXPORTCOMMON_H
#define KIFUEXPORTCOMMON_H

/// @file kifuexportcommon.h
/// @brief 棋譜エクスポータ（KIF/KI2/CSA/USI/USEN）で共通の文字列処理

#include <QString>
#include <QStringList>

namespace KifuExportCommon {

/// 「キー：値」の対局情報行（全角コロン）
QString fwColonLine(const QString& key, const QString& value);

/// 指し手の先頭の手番記号（▲△）を除く
QString removeTurnMarker(const QString& move);

/// 終局を表す指し手（投了・詰み・千日手・反則・引き分け・最大手数など）か
bool isTerminalMove(const QString& move);

/// しおり（改行区切り）を KIF/KI2 の「&名前」行として足す
void appendKifBookmarks(const QString& bookmark, QStringList& out);

/// コメント（改行区切り）を KIF/KI2 の「*本文」行として足す（後方互換の【しおり】は & 行にする）
void appendKifComments(const QString& comment, QStringList& out);

/**
 * @brief KIF/KI2 の終局行（「まで N手で…」）を作る
 * @param lastActualMoveNo 終局語を除いた最後の指し手の手数
 * @param terminalMove 終局語の指し手（手番記号付き。空なら手数だけの行）
 * @param initialBlackToMove 開始局面が先手番か
 * @param handicapNames 駒落ちの呼び方（下手・上手）で書くか
 *
 * Kifu for Windows と同じく、投了は「まで N手で先手の勝ち」、詰みは「まで N手で詰み」と書き、
 * 時間切れ・反則・入玉宣言・不戦などは終局の種類を明記する（KI2 は終局語の行がないため、
 * この行から終局の種類を読み戻す）。
 */
QString buildEndingLine(int lastActualMoveNo, const QString& terminalMove, bool initialBlackToMove,
                        bool handicapNames);

} // namespace KifuExportCommon

#endif // KIFUEXPORTCOMMON_H
