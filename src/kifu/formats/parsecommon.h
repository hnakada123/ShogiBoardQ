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
#include "terminaltype.h"
#include "shogitypes.h"

namespace KifuParseCommon {

/// 形式固有の字句読み取り中だけ使う。公開結果は resolve() で1手単位にまとめる。
struct KifLineDraft {
    int startPly = 1;
    QString baseSfen;
    QStringList usiMoves;
    QList<KifDisplayItem> disp;
    bool endsWithTerminal = false;
    QString previousUsi;
    bool resolve(KifLine& out, QString* error) const;
};

struct MoveDestination {
    int toFile;
    int toRank;
    bool isSameAsPrev;
};

/**
 * @brief 読めない手・指せない手の扱い（全形式で同じ）
 *
 * 読めない手（表記が解釈できない・局面に合わない）や指せない手があれば、その手順はその手の手前で打ち切り、
 * 警告に理由と「何手目まで読んだか」を書いて読み込みを続ける。パスも対応していない手として同じに扱う。
 * ply はその手の手数
 */
QString truncationNotice(int ply);
/// パスの手で手順を打ち切ったときの警告（truncationNotice を含む）
QString passNotice(int ply);

// 改行区切りで追記（buf が空でなければ '\n' を挿入してから text を追加）
void appendLine(QString& buf, const QString& text);

// 直前の手（items.last()）にコメントを付与し buf をクリア
// items.size() > minSize のときのみ付与する
void flushCommentToLastItem(QString& buf, QList<KifDisplayItem>& items, int minSize = 1);

// 開始局面用 KifDisplayItem を生成
KifDisplayItem createOpeningDisplayItem(const QString& comment, const QString& bookmark);

// KIF/KI2共通：盤面枠行・ヘッダ行を判定（BODの数字行、罫線行、持駒見出し等）
bool isBoardHeaderOrFrame(QStringView line);

// KIF/KI2共通：スキップ可能なヘッダ行かどうか判定
bool isKifSkippableHeaderLine(QStringView line);

// KIF/KI2共通：コメント行判定（先頭が '*' or '＊'）
bool isKifCommentLine(QStringView s);

// KIF/KI2共通：しおり行判定（先頭が '&'）
bool isBookmarkLine(QStringView s);

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

// 終局の表示アイテムを生成する。表記は終局語の表（TerminalTypes）から作る
KifDisplayItem createTerminalDisplayItem(int ply, TerminalType type, Turn side,
                                         const QString& timeText = QString());

// 指し手の表示アイテムを生成
KifDisplayItem createMoveDisplayItem(int ply, const QString& prettyMove,
                                     const QString& timeText = QString());

// 指し手リストの後処理（残コメントフラッシュ＋開始局面アイテム保証）
void finalizeDisplayItems(QString& commentBuf, QList<KifDisplayItem>& items,
                          const QString& openingCommentBuf, const QString& openingBookmarkBuf);

// 持駒の枚数からSFENの持駒欄を作る（飛角金銀桂香歩の順。先手は大文字、後手は小文字。なければ "-"）
QString buildHandsSfen(const QMap<Piece, int>& black, const QMap<Piece, int>& white);

// KIF/KI2共通：手順の先頭にある開始局面の項目を取り除き、そのコメント・しおりを最初の指し手へ移す
// （変化の手順は最初の指し手から始まる）
void mergeOpeningIntoFirstMove(KifLine& line);

} // namespace KifuParseCommon

// USI/USEN指し手フォーマット関数は parsemoveformat.h に移動
#include "parsemoveformat.h"

#endif // PARSECOMMON_H
