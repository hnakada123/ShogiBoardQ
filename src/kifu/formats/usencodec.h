#ifndef USENCODEC_H
#define USENCODEC_H

/// @file usencodec.h
/// @brief USEN の符号（指し手の3文字・開始局面・1本の手順）の符号化と復号

#include <QString>
#include <QStringList>
#include <QStringView>

/// @brief USEN の手順の復号結果（エラー情報付き）
struct UsenDecodeResult {
    QStringList moves;       ///< 復号した USI の指し手（失敗が1つでもあれば空）
    int invalidCount = 0;    ///< 復号できなかった手の数
    QString firstError;      ///< 最初のエラー内容（失敗がなければ空）
};

/**
 * @brief USEN の符号表（読み込みの UsenToSfenConverter と書き出しの UsenExporter で共有する）
 *
 * - 指し手: code = (from * 81 + to) * 2 + 成り。マスは (段 - 1) * 9 + (筋 - 1)、駒打ちの from は 81 + 駒のコード
 *   （金 9・歩 10・香 11・桂 12・銀 13・角 14・飛 15）。code を base36 の3文字で書く
 * - 開始局面: SFEN の手数を除き、'/' を '_'、' ' を '.'、'+' を 'z' にする。平手は空
 * - 1本の手順: "~<オフセット>.<指し手の3文字の列>.<終局コード>"
 */
namespace UsenCodec {

/// USI の指し手を3文字にする（読めない指し手は空）
QString encodeMove(const QString& usiMove);
/// 3文字を USI の指し手にする（読めなければ空）
QString decodeMove(QStringView threeChars);

/// 開始局面を "~" の前の部分にする（平手と空は空）
QString encodePosition(const QString& sfen);
/// "~" の前の部分を SFEN にする（手数を省いていれば 1 を補う。空なら空）
QString decodePosition(QStringView part);

/// 手順の部分（"~0.xxxyyy.r" または "xxxyyy.r"）を復号する。終局コードは *terminal に返す
UsenDecodeResult decodeMoves(const QString& usen, QString* terminal = nullptr);

} // namespace UsenCodec

#endif // USENCODEC_H
