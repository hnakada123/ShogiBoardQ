#ifndef POSITIONCOLLECTIONFILE_H
#define POSITIONCOLLECTIONFILE_H

/// @file positioncollectionfile.h
/// @brief 局面集ファイル（1行1局面）の読み取り。局面集ビューアと詰将棋問題一覧で共有する

#include <QList>
#include <QString>
#include <QStringList>

/**
 * @brief 局面集ファイルの1行の規則
 *
 * - 空行と `#` で始まる行（詰将棋局面生成が先頭に付けるコメント）は読み飛ばす。行頭の BOM と行末の CR は除く
 * - 局面は SFEN の4語（前に `sfen` を付けてもよい）か `startpos`。前に `position` を付けた USI の
 *   position コマンドも受け付ける
 * - 局面の後には `moves` と USI の指し手を続けられる。position コマンドの指し手は「指した後の局面」を表し、
 *   SFEN だけの行の指し手は「その局面からの参考手順」（詰将棋局面生成が保存した解答手順）を表す
 * - 局面は SfenValidator で確かめる。それ以外の語が続く行・指し手の形でない語がある行は読めない行
 */
namespace PositionCollectionFile {

/// 読めた1行
struct Line {
    int lineNumber = 0;      ///< ファイルの行番号（1始まり）
    QString startSfen;       ///< 書かれた局面（SFEN の4語。startpos は平手の SFEN）
    QStringList moves;       ///< moves の後の指し手
    bool movesPlayed = false; ///< position コマンドの指し手（true）か、参考手順（false）か

    /// この行が表す局面（position コマンドなら指した後の局面）の正規化した SFEN。指せない手があれば空
    QString position() const;
    /// 参考手順（position コマンドの行では空）
    QStringList referenceMoves() const { return movesPlayed ? QStringList() : moves; }
};

/// ファイル全体の読み取り結果
struct Contents {
    QList<Line> lines;       ///< 読めた行（ファイルの順）
    QList<int> invalidLines; ///< 局面として読めなかった行の番号
};

enum class LineKind { Skipped, Position, Invalid };

/// 1行を読む。読めた行なら Position を返して *line に書く
LineKind parseLine(const QString& text, int lineNumber, Line* line);
/// テキスト全体を読む
Contents parse(const QString& text);

} // namespace PositionCollectionFile

#endif // POSITIONCOLLECTIONFILE_H
