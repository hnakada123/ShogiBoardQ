#ifndef SHIFTJISCODEC_H
#define SHIFTJISCODEC_H

/// @file shiftjiscodec.h
/// @brief Shift_JIS（CP932）の変換機能の定義

#include <QByteArray>
#include <QString>

#include <optional>

/**
 * @brief Shift_JIS（CP932）とQStringの相互変換
 *
 * QtのQStringConverterはICUが無いビルド（macOS/Windowsの公式バイナリなど）では
 * Shift_JISを扱えないため、その場合はOSの変換機能（iconv / Win32 API）を使う。
 */
namespace ShiftJisCodec {

/// Shift_JISのバイト列を変換する。不正なバイト列や変換手段が無い場合は nullopt
std::optional<QString> decode(const QByteArray& bytes);

/// Shift_JISへ変換する。表現できない文字を含む場合や変換手段が無い場合は nullopt
std::optional<QByteArray> encode(const QString& text);

/// Qtを使わずOSの変換機能だけで変換する（テスト用）
std::optional<QString> decodeWithPlatform(const QByteArray& bytes);
std::optional<QByteArray> encodeWithPlatform(const QString& text);

} // namespace ShiftJisCodec

#endif // SHIFTJISCODEC_H
