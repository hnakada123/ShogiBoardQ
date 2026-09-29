/// @file shiftjiscodec.cpp
/// @brief Shift_JIS（CP932）の変換機能の実装

#include "shiftjiscodec.h"

#include <QStringDecoder>
#include <QStringEncoder>

#ifdef Q_OS_WIN
#include <limits>
#include <qt_windows.h>
#else
#include <iconv.h>
#endif

namespace ShiftJisCodec {

namespace {

#ifdef Q_OS_WIN

constexpr UINT kCodePage932 = 932;

#else

// from の文字コードの bytes を to へ変換する。失敗時は nullopt
std::optional<QByteArray> convertWithIconv(const char* to, const char* from,
                                           const QByteArray& bytes, qsizetype maxGrowth)
{
    iconv_t cd = iconv_open(to, from);
    if (cd == reinterpret_cast<iconv_t>(-1)) return std::nullopt;  // NOLINT(performance-no-int-to-ptr): iconv_open の失敗値

    QByteArray input = bytes;
    QByteArray output(bytes.size() * maxGrowth + 4, Qt::Uninitialized);
    char* in = input.data();
    char* out = output.data();
    size_t inLeft = static_cast<size_t>(input.size());
    size_t outLeft = static_cast<size_t>(output.size());

    const size_t result = iconv(cd, &in, &inLeft, &out, &outLeft);
    iconv_close(cd);
    if (result == static_cast<size_t>(-1) || inLeft != 0) return std::nullopt;

    output.truncate(output.size() - static_cast<qsizetype>(outLeft));
    return output;
}

#endif

} // namespace

std::optional<QString> decodeWithPlatform(const QByteArray& bytes)
{
    if (bytes.isEmpty()) return QString();
#ifdef Q_OS_WIN
    if (bytes.size() > std::numeric_limits<int>::max()) return std::nullopt;
    const int inSize = static_cast<int>(bytes.size());
    const int length = MultiByteToWideChar(kCodePage932, MB_ERR_INVALID_CHARS,
                                           bytes.constData(), inSize, nullptr, 0);
    if (length <= 0) return std::nullopt;
    QString text(length, Qt::Uninitialized);
    if (MultiByteToWideChar(kCodePage932, MB_ERR_INVALID_CHARS, bytes.constData(), inSize,
                            reinterpret_cast<wchar_t*>(text.data()), length) != length) {
        return std::nullopt;
    }
    return text;
#else
    // CP932 の1バイトは UTF-8 で最大3バイトになる
    const auto utf8 = convertWithIconv("UTF-8", "CP932", bytes, 3);
    if (!utf8) return std::nullopt;
    return QString::fromUtf8(*utf8);
#endif
}

std::optional<QByteArray> encodeWithPlatform(const QString& text)
{
    if (text.isEmpty()) return QByteArray();
#ifdef Q_OS_WIN
    if (text.size() > std::numeric_limits<int>::max()) return std::nullopt;
    const auto* wide = reinterpret_cast<const wchar_t*>(text.utf16());
    const int inSize = static_cast<int>(text.size());
    BOOL usedDefault = FALSE;
    const int length = WideCharToMultiByte(kCodePage932, WC_NO_BEST_FIT_CHARS, wide, inSize,
                                           nullptr, 0, nullptr, &usedDefault);
    if (length <= 0 || usedDefault) return std::nullopt;
    QByteArray bytes(length, Qt::Uninitialized);
    if (WideCharToMultiByte(kCodePage932, WC_NO_BEST_FIT_CHARS, wide, inSize,
                            bytes.data(), length, nullptr, &usedDefault) != length
        || usedDefault) {
        return std::nullopt;
    }
    return bytes;
#else
    // UTF-8 の各文字は CP932 で同じかそれより短くなる
    return convertWithIconv("CP932", "UTF-8", text.toUtf8(), 1);
#endif
}

std::optional<QString> decode(const QByteArray& bytes)
{
    QStringDecoder decoder("Shift-JIS");
    if (!decoder.isValid()) return decodeWithPlatform(bytes);

    QString text = decoder.decode(bytes);
    if (decoder.hasError() || text.contains(QChar::ReplacementCharacter)) return std::nullopt;
    return text;
}

std::optional<QByteArray> encode(const QString& text)
{
    QStringEncoder encoder("Shift-JIS");
    if (!encoder.isValid()) return encodeWithPlatform(text);

    QByteArray bytes = encoder.encode(text);
    if (encoder.hasError()) return std::nullopt;
    return bytes;
}

} // namespace ShiftJisCodec
