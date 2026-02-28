#ifndef PARSECOMMON_H
#define PARSECOMMON_H

#include <QChar>
#include <QList>
#include <QMap>
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <QStringView>
#include <array>
#include <functional>
#include <optional>
#include "kifdisplayitem.h"
#include "kifparsetypes.h"
#include "shogitypes.h"

class SfenPositionTracer;

namespace KifuParseCommon {

int asciiDigitToInt(QChar c);
int zenkakuDigitToInt(QChar c);
int flexDigitToIntNoDetach(QChar c);
int flexDigitsToIntNoDetach(QStringView text);

const std::array<QString, 16>& terminalWords();
bool isTerminalWordExact(QStringView text, QString* normalized = nullptr);
bool isTerminalWordContains(QStringView text, QString* normalized = nullptr);

bool mapKanjiPiece(QStringView text, Piece& base, bool& promoted);

[[nodiscard]] std::optional<int> parseFileChar(QChar ch);   // '1'〜'9' → 1〜9
[[nodiscard]] std::optional<int> parseRankChar(QChar ch);   // 'a'〜'i' → 1〜9
[[nodiscard]] std::optional<int> parseDigit(QChar ch);      // '0'〜'9' → 0〜9

// 改行区切りで追記（buf が空でなければ '\n' を挿入してから text を追加）
void appendLine(QString& buf, const QString& text);

// 直前の手（items.last()）にコメントを付与し buf をクリア
// items.size() > minSize のときのみ付与する
void flushCommentToLastItem(QString& buf, QList<KifDisplayItem>& items, int minSize = 1);

// 開始局面用 KifDisplayItem を生成
KifDisplayItem createOpeningDisplayItem(const QString& comment, const QString& bookmark);

// KIF/KI2共通：盤面枠行・ヘッダ行を判定（BODの数字行、罫線行、持駒見出し等）
bool isBoardHeaderOrFrame(QStringView line);

// KIF/KI2共通：終局語を含むか判定
bool containsAnyTerminal(QStringView s, QString* matched = nullptr);

// KIF/KI2共通：スキップ可能なヘッダ行かどうか判定
bool isKifSkippableHeaderLine(QStringView line);

// KIF/KI2共通：コメント行判定（先頭が '*' or '＊'）
bool isKifCommentLine(QStringView s);

// KIF/KI2共通：しおり行判定（先頭が '&'）
bool isBookmarkLine(QStringView s);

// CSA/JKF共通：CSA形式終局コード→日本語ラベル
// %プレフィックスの有無を自動判別（%TORYO / TORYO 両対応）
// マッチしない場合は入力文字列をそのまま返す
QString csaSpecialToJapanese(const QString& code);

// CSA/JKF共通：mm:ss形式の時間フォーマット（ミリ秒入力）
QString formatTimeMS(qint64 ms);

// CSA/JKF共通：HH:MM:SS形式の時間フォーマット（ミリ秒入力）
QString formatTimeHMS(qint64 ms);

// CSA/JKF共通：mm:ss/HH:MM:SS形式の複合時間フォーマット
QString formatTimeText(qint64 moveMs, qint64 cumMs);

// KIF/KI2/JKF共通：extractGameInfo結果をQMap変換
QMap<QString, QString> toGameInfoMap(const QList<KifGameInfoItem>& items);

// 全コンバータ共通：手番マーク生成（奇数手=▲、偶数手=△）
inline QString tebanMark(int ply) {
    return (ply % 2 != 0) ? QStringLiteral("▲") : QStringLiteral("△");
}

// KIF/KI2共通：BOD持駒行判定（先手の持駒/後手の持駒/持ち駒）
bool isBodHandsLine(QStringView line);

// KIF/KI2共通：ヘッダ行から対局情報を抽出
// isMoveLine: 指し手行を検出したら true を返す（形式により判定方法が異なる）
QList<KifGameInfoItem> extractHeaderGameInfo(
    const QStringList& lines,
    const std::function<bool(const QString&)>& isMoveLine);

// KIF/KI2共通：コメント行の処理（内容をバッファに追加）
// コメント行であれば true を返す
bool tryHandleCommentLine(const QString& line, bool firstMoveFound,
                          QString& commentBuf, QString& openingCommentBuf);

// KIF/KI2共通：しおり行の処理（バッファまたは直前アイテムに追加）
// しおり行であれば true を返す
bool tryHandleBookmarkLine(const QString& line, bool firstMoveFound,
                           QList<KifDisplayItem>& items, QString& openingBookmarkBuf);

// 終局語の表示アイテムを生成（時間情報なしの汎用版）
KifDisplayItem createTerminalDisplayItem(int ply, const QString& term,
                                         const QString& timeText = QString());

// 指し手の表示アイテムを生成
KifDisplayItem createMoveDisplayItem(int ply, const QString& prettyMove,
                                     const QString& timeText = QString());

// 指し手リストの後処理（残コメントフラッシュ＋開始局面アイテム保証）
void finalizeDisplayItems(QString& commentBuf, QList<KifDisplayItem>& items,
                          const QString& openingCommentBuf, const QString& openingBookmarkBuf);

// USI/USEN共通：USI駒文字から漢字駒名を取得 (P→歩, L→香, etc.)
QString usiPieceToKanji(QChar usiPiece);

// USI/USEN共通：盤面トークンから漢字駒名を取得（成駒対応: +P→と, +B→馬 etc.）
QString usiTokenToKanji(QStringView token);

// USI/USEN共通：USI指し手から駒トークンを抽出
// tracer: 現在の盤面状態を追跡するトレーサー（指し手適用前の状態）
QString extractUsiPieceToken(QStringView usi, SfenPositionTracer& tracer);

// USI/USEN共通：USI指し手から日本語表記を生成
// pieceToken: SfenPositionTracerから取得した駒トークン（例: "P", "+B"）
QString usiMoveToPretty(const QString& usi, int plyNumber,
                        int& prevToFile, int& prevToRank,
                        const QString& pieceToken);

// USI/USEN共通：USI指し手列から表示アイテムを構築
// SfenPositionTracerで盤面追跡しながら日本語指し手表記を生成
// outDisp に指し手アイテムを追加（開始局面・終局アイテムは含まない）
// 戻り値は最後のply番号
int buildUsiMoveDisplayItems(const QStringList& usiMoves,
                             const QString& baseSfen,
                             int startPly,
                             QList<KifDisplayItem>& outDisp);

} // namespace KifuParseCommon

#endif // PARSECOMMON_H
