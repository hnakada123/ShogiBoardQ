/// @file kifutextdecoder.cpp
/// @brief 棋譜ファイルの文字コード自動判別読み込み機能の実装

#include "kifutextdecoder.h"
#include "boundedfile.h"
#include "kifuformat.h"

#include <QByteArray>
#include "logcategories.h"
#include "shiftjiscodec.h"
#include <QStringDecoder>

#include <optional>

namespace KifuTextDecoder {

// --- helpers ---------------------------------------------------------------

static inline void normalizeNewlines(QString& s)
{
    // CRLF -> LF、残った CR も LF へ
    s.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    s.replace(QChar(u'\r'), QChar(u'\n'));
}

static inline void splitByNewlines(const QString& s, QStringList& out)
{
    QString t = s;
    // BOM を除去（行頭のみ対象）
    if (!t.isEmpty() && t.front() == QChar(0xFEFF)) {
        t.remove(0, 1);
    }
    normalizeNewlines(t);
    out = t.split(QChar(u'\n'), Qt::KeepEmptyParts);
}

// QByteArray を指定の codecName でデコードして戻す。
// 成功の目安として U+FFFD（置換文字）が存在しないことを条件に ok を立てます。
static inline QString decodeWith(const QByteArray& bytes, const char* codecName, bool& ok)
{
    QStringDecoder dec(codecName);
    // ICU なしの Qt では UTF 以外のデコーダが無効になり、空文字列を返すため失敗扱いにする
    if (!dec.isValid()) {
        ok = false;
        return QString();
    }
    QString s = dec.decode(bytes);
    ok = !dec.hasError() && !s.contains(QChar(0xFFFD));
    return s;
}

// BOM 判定（UTF-8 / UTF-16LE / UTF-16BE / UTF-32LE / UTF-32BE）
// 見つかった場合は usedEncoding を設定し、適切にデコードします。
static bool decodeByBom(const QByteArray& bytes, QString& out, QString* usedEncoding)
{
    const auto sz = bytes.size();
    const auto data = reinterpret_cast<const unsigned char*>(bytes.constData());

    if (sz >= 3 &&
        data[0] == 0xEF &&
        data[1] == 0xBB &&
        data[2] == 0xBF) {
        bool ok = true;
        out = decodeWith(bytes.mid(3), "UTF-8", ok);
        if (usedEncoding) *usedEncoding = QStringLiteral("utf-8(bom)");
        return true;
    }
    if (sz >= 4 &&
        data[0] == 0xFF &&
        data[1] == 0xFE &&
        data[2] == 0x00 &&
        data[3] == 0x00) {
        // UTF-32LE
        QStringDecoder dec(QStringConverter::Utf32LE);
        out = dec.decode(bytes.mid(4));
        if (usedEncoding) *usedEncoding = QStringLiteral("utf-32le(bom)");
        return true;
    }
    if (sz >= 4 &&
        data[0] == 0x00 &&
        data[1] == 0x00 &&
        data[2] == 0xFE &&
        data[3] == 0xFF) {
        // UTF-32BE
        QStringDecoder dec(QStringConverter::Utf32BE);
        out = dec.decode(bytes.mid(4));
        if (usedEncoding) *usedEncoding = QStringLiteral("utf-32be(bom)");
        return true;
    }
    if (sz >= 2 &&
        data[0] == 0xFF &&
        data[1] == 0xFE) {
        // UTF-16LE
        QStringDecoder dec(QStringConverter::Utf16LE);
        out = dec.decode(bytes.mid(2));
        if (usedEncoding) *usedEncoding = QStringLiteral("utf-16le(bom)");
        return true;
    }
    if (sz >= 2 &&
        data[0] == 0xFE &&
        data[1] == 0xFF) {
        // UTF-16BE
        QStringDecoder dec(QStringConverter::Utf16BE);
        out = dec.decode(bytes.mid(2));
        if (usedEncoding) *usedEncoding = QStringLiteral("utf-16be(bom)");
        return true;
    }
    return false;
}

// 文字コードの宣言を探す範囲（宣言は先頭の行に書く）
constexpr qsizetype kDeclarationBytes = 128;
constexpr char kDeclarationKey[] = "encoding=";

// 先頭の範囲にあるコメントの行（' か # で始まる）の "encoding=" の宣言。KIF・KI2 の
// "#KIF version=2.0 encoding=Shift_JIS"、CSA の "'CSA encoding=UTF-8"（V3.0）と "'encoding=UTF-8"。
// 値は UTF-8 と Shift_JIS だけを読み（大文字・小文字は問わない）、ほかの値は宣言が無いものとする
static std::optional<KifuEncoding> declaredEncoding(const QByteArray& bytes)
{
    const QList<QByteArray> lines = bytes.left(kDeclarationBytes).split('\n');
    for (const QByteArray& rawLine : lines) {
        const QByteArray line = rawLine.trimmed();
        if (!line.startsWith('#') && !line.startsWith('\'')) continue;
        const QList<QByteArray> words = line.mid(1).simplified().split(' ');
        for (const QByteArray& word : words) {
            const QByteArray lower = word.toLower();
            if (!lower.startsWith(kDeclarationKey)) continue;
            const QByteArray value = lower.mid(qsizetype(sizeof(kDeclarationKey) - 1));
            if (value == "utf-8") return KifuEncoding::Utf8;
            if (value == "shift_jis") return KifuEncoding::ShiftJis;
            return std::nullopt;
        }
    }
    return std::nullopt;
}

// 宣言の文字コードで読む（読めなければ nullopt）
static std::optional<QString> decodeAs(const QByteArray& bytes, KifuEncoding encoding)
{
    if (encoding == KifuEncoding::ShiftJis) return ShiftJisCodec::decode(bytes);
    bool ok = false;
    QString text = decodeWith(bytes, "UTF-8", ok);
    if (!ok) return std::nullopt;
    return text;
}

// --- public API ------------------------------------------------------------

QString decodeAuto(const QByteArray& bytes, QString* usedEncoding, QString* warn)
{
    QString text;
    // 1) BOM 優先
    if (decodeByBom(bytes, text, usedEncoding)) return text;
    // 2) 文字コードの宣言。宣言どおりに読めないときは宣言を使わずに判別する
    if (const std::optional<KifuEncoding> declared = declaredEncoding(bytes)) {
        if (std::optional<QString> decoded = decodeAs(bytes, *declared)) {
            if (usedEncoding) {
                *usedEncoding = *declared == KifuEncoding::ShiftJis ? QStringLiteral("cp932(declared)")
                                                                     : QStringLiteral("utf-8(declared)");
            }
            return std::move(*decoded);
        }
    }
    // 3) UTF-8 を試す（置換文字の有無で判定）
    bool okUtf8 = false;
    QString sUtf8 = decodeWith(bytes, "UTF-8", okUtf8);
    if (okUtf8) {
        if (usedEncoding) *usedEncoding = QStringLiteral("utf-8");
        return sUtf8;
    }
    // 4) CP932 / Shift_JIS
    if (auto sSj = ShiftJisCodec::decode(bytes)) {
        if (usedEncoding) *usedEncoding = QStringLiteral("cp932");
        return std::move(*sSj);
    }
    // 5) EUC-JP（稀）
    bool okEuc = false;
    QString sEuc = decodeWith(bytes, "EUC-JP", okEuc);
    if (okEuc) {
        if (usedEncoding) *usedEncoding = QStringLiteral("euc-jp");
        return sEuc;
    }
    // 6) 仕方なくローカル 8bit
    if (usedEncoding) *usedEncoding = QStringLiteral("local8bit(fallback)");
    if (warn) *warn += QStringLiteral("fallback: local8bit was used; encoding could not be detected precisely.\n");
    return QString::fromLocal8Bit(bytes);
}

bool readAllTextAuto(const QString& filePath,
                     QString& outText,
                     QString* usedEncoding,
                     QString* warn)
{
    outText.clear();

    QByteArray bytes;
    const BoundedFile::Result read = BoundedFile::read(filePath, BoundedFile::kMaxKifuBytes, bytes);
    if (read != BoundedFile::Result::Ok) {
        if (warn) *warn += BoundedFile::describe(read, filePath, BoundedFile::kMaxKifuBytes) + QLatin1Char('\n');
        return false;
    }
    QString encoding;
    outText = decodeAuto(bytes, &encoding, warn);
    qCDebug(lcKifu).noquote() << QStringLiteral("readAllTextAuto: encoding = %1, bytes = %2").arg(encoding).arg(bytes.size());
    if (usedEncoding) *usedEncoding = encoding;
    return true;
}

bool readAllLinesAuto(const QString& filePath,
                      QStringList& outLines,
                      QString* usedEncoding,
                      QString* warn)
{
    outLines.clear();
    QString text;
    if (!readAllTextAuto(filePath, text, usedEncoding, warn)) return false;
    outLines = splitLines(text);
    return true;
}

QStringList splitLines(const QString& text)
{
    QStringList lines;
    splitByNewlines(text, lines);
    return lines;
}

} // namespace KifuTextDecoder
