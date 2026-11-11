#ifndef P2PDISPLAYNAME_H
#define P2PDISPLAYNAME_H

/// @file p2pdisplayname.h
/// @brief P2P対局の表示名の規則（protocol/displayname-v1.txt の表だけで判定する。実装仕様 3.2節）

#include <QByteArrayView>
#include <QString>

namespace P2p {

namespace DisplayName {

/// UTF-8 のバイト数の上限
inline constexpr int kMaxBytes = 96;
/// 文字（コードポイント）の数の上限
inline constexpr int kMaxCodePoints = 32;

/// UTF-8 のバイト列が表示名の規則を満たすか（通信プロセスと同じ表・同じ判定）
bool isValidUtf8(QByteArrayView utf8);
/// 文字列が表示名の規則を満たすか
bool isValid(const QString& name);
/// 文字（コードポイント）が表にあるか
bool isAllowedCodePoint(char32_t codePoint);

/**
 * @brief 名前を比べるための形（実装仕様 3.4節）
 *
 * 全角の英数字と記号を半角に、英字の大文字を小文字にそろえる。見た目が似た別の文字までは
 * そろえないので、相手を確かめる根拠にはしない。
 */
QString comparisonKey(const QString& name);

} // namespace DisplayName

} // namespace P2p

#endif // P2PDISPLAYNAME_H
