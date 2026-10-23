#ifndef USENCODEC_H
#define USENCODEC_H

/// @file usencodec.h
/// @brief USEN の符号（指し手の3文字・開始局面・手順）の符号化と復号

#include <QList>
#include <QString>
#include <QStringView>
#include <optional>

#include "usimove.h"

/// USEN の1本の手順（"~" の後の "<オフセット>.<指し手の3文字の列>[.<終局コード>]"）
struct UsenLine {
    int offset = 0;          ///< 何手の後から始まるか（本譜は 0。変化はその手が変える手の1つ前の手数）
    QList<UsiMove> moves;
    QString terminal;        ///< 終局コード（TerminalCodes::fromUsen。無ければ空）
};

/**
 * @brief USEN の符号表（読み込みの UsenToSfenConverter と書き出しの UsenExporter で共有する）
 *
 * - 指し手: code = (from * 81 + to) * 2 + 成り。マスは (段 - 1) * 9 + (筋 - 1)、駒打ちの from は 81 + 駒のコード
 *   （金 9・歩 10・香 11・桂 12・銀 13・角 14・飛 15）。code を base36 の3文字で書く
 * - 開始局面: SFEN の手数を除き、'/' を '_'、' ' を '.'、'+' を 'z' にする。平手は空
 * - 棋譜: "<開始局面>~<本譜>~<変化>~..."。手順は "<オフセット>.<指し手の3文字の列>.<終局コード>"
 */
namespace UsenCodec {

/// 指し手を3文字にする（符号にできない指し手は空）
QString encodeMove(const UsiMove& move);
/// 3文字を指し手にする（読めなければ nullopt）
std::optional<UsiMove> decodeMove(QStringView threeChars);

/// 開始局面を "~" の前の部分にする（平手と空は空）
QString encodePosition(const QString& sfen);
/// "~" の前の部分を SFEN にする（手数を省いていれば 1 を補う。空なら空）
QString decodePosition(QStringView part);

/// 1本の手順（"<オフセット>.<指し手の3文字の列>[.<終局コード>]"）を読む。1手でも読めなければ nullopt と error
std::optional<UsenLine> decodeLine(QStringView part, QString* error = nullptr);
/// 棋譜の "~" より後（最初が本譜、続きが変化）を読む。開始局面の部分は読まない。1本でも読めなければ nullopt と error
std::optional<QList<UsenLine>> decodeLines(QStringView usen, QString* error = nullptr);

} // namespace UsenCodec

#endif // USENCODEC_H
