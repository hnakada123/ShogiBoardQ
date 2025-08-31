#include "kifreader.h"

#include <QFile>
#include <QRegularExpression>

#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
  #include <QStringDecoder>
#else
  #include <QTextCodec>
#endif

#include <QByteArray>
#include <QVector>

#ifdef Q_OS_WIN
  #define NOMINMAX
  #include <windows.h>
#else
  // POSIX iconv fallback (Linux, macOS など)
  #include <iconv.h>
  #include <errno.h>
#endif

namespace {

inline bool hasReplacementChar(const QString& s) { return s.contains(QChar(0xFFFD)); }

#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
static bool decodeWithName_Qt6(const QByteArray& bytes, const char* name, QString& out)
{
    auto enc = QStringConverter::encodingForName(name);
    if (!enc) return false;
    QStringDecoder dec(*enc);
    out = dec.decode(bytes);
    if (dec.hasError()) return false;
    return !hasReplacementChar(out);
}
#else
static bool decodeWithName_Qt5(const QByteArray& bytes, const char* name, QString& out)
{
    QTextCodec* codec = QTextCodec::codecForName(name);
    if (!codec) return false;
    out = codec->toUnicode(bytes);
    return !hasReplacementChar(out);
}
#endif

// ----------------- Shift_JIS / CP932 専用フォールバック -----------------

// Windows：WinAPI で CP932 変換
static bool decodeShiftJIS_Win(const QByteArray& bytes, QString& out)
{
#ifdef Q_OS_WIN
    if (bytes.isEmpty()) { out.clear(); return true; }
    const int wlen = MultiByteToWideChar(932 /*CP932*/, MB_ERR_INVALID_CHARS,
                                         bytes.constData(), bytes.size(), nullptr, 0);
    if (wlen <= 0) return false;
    QString tmp(wlen, QChar());
    int ok = MultiByteToWideChar(932, MB_ERR_INVALID_CHARS,
                                 bytes.constData(), bytes.size(),
                                 reinterpret_cast<wchar_t*>(tmp.data()), wlen);
    if (ok <= 0) return false;
    out = tmp;
    return !hasReplacementChar(out);
#else
    Q_UNUSED(bytes)
    Q_UNUSED(out)
    return false;
#endif
}

// POSIX：iconv で CP932/SJIS → UTF-8 → QString
#ifndef Q_OS_WIN
static bool iconvDecodeToUtf8(const QByteArray& in, const char* from, QByteArray& outUtf8)
{
    iconv_t cd = iconv_open("UTF-8", from);
    if (cd == (iconv_t)-1) {
        return false;
    }
    // 出力は最大でもおおよそ 4 倍で足りるが、余裕を見て 6 倍＋α
    size_t inLeft = static_cast<size_t>(in.size());
    size_t outCap = inLeft * 6 + 32;
    outUtf8.resize(static_cast<int>(outCap));

    char* inPtr = const_cast<char*>(in.constData());
    char* outPtr = outUtf8.data();
    size_t outLeft = outCap;

    while (inLeft > 0) {
        size_t res = iconv(cd, &inPtr, &inLeft, &outPtr, &outLeft);
        if (res == (size_t)-1) {
            if (errno == E2BIG) {
                // バッファ拡張
                size_t used = outCap - outLeft;
                outCap *= 2;
                outUtf8.resize(static_cast<int>(outCap));
                outPtr = outUtf8.data() + used;
                outLeft = outCap - used;
                continue;
            } else {
                iconv_close(cd);
                return false; // 不正シーケンス等
            }
        }
    }
    iconv_close(cd);
    size_t used = outCap - outLeft;
    outUtf8.resize(static_cast<int>(used));
    return true;
}

static bool decodeShiftJIS_Iconv(const QByteArray& bytes, QString& out)
{
    // CP932 を優先（Shift_JIS の超集合）。ダメなら各種別名を総当たり。
    static const char* const kNames[] = {
        "CP932", "SHIFT_JIS", "Shift_JIS", "SJIS", "Shift-JIS", "Windows-31J", "MS_Kanji"
    };
    QByteArray utf8;
    for (const char* name : kNames) {
        if (iconvDecodeToUtf8(bytes, name, utf8)) {
            out = QString::fromUtf8(utf8.constData(), utf8.size());
            if (!hasReplacementChar(out)) return true;
        }
    }
    return false;
}
#endif // !Q_OS_WIN

// ----------------- ここまで SJIS フォールバック -----------------

// BOM 判定（UTF-8/UTF-16LE/UTF-16BE）
static bool decodeWithBom(const QByteArray& bytes, QString& out, QString* usedEncoding)
{
    if (bytes.size() >= 3 &&
        (uchar)bytes[0]==0xEF && (uchar)bytes[1]==0xBB && (uchar)bytes[2]==0xBF) {
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
        if (decodeWithName_Qt6(bytes.mid(3), "UTF-8", out)) {
#else
        if (decodeWithName_Qt5(bytes.mid(3), "UTF-8", out)) {
#endif
            if (usedEncoding) *usedEncoding = QStringLiteral("utf-8(bom)");
            return true;
        }
    }
    if (bytes.size() >= 2 &&
        (uchar)bytes[0]==0xFF && (uchar)bytes[1]==0xFE) { // UTF-16LE
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
        if (decodeWithName_Qt6(bytes.mid(2), "UTF-16LE", out)) {
#else
        if (decodeWithName_Qt5(bytes.mid(2), "UTF-16LE", out)) {
#endif
            if (usedEncoding) *usedEncoding = QStringLiteral("utf-16le(bom)");
            return true;
        }
    }
    if (bytes.size() >= 2 &&
        (uchar)bytes[0]==0xFE && (uchar)bytes[1]==0xFF) { // UTF-16BE
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
        if (decodeWithName_Qt6(bytes.mid(2), "UTF-16BE", out)) {
#else
        if (decodeWithName_Qt5(bytes.mid(2), "UTF-16BE", out)) {
#endif
            if (usedEncoding) *usedEncoding = QStringLiteral("utf-16be(bom)");
            return true;
        }
    }
    return false;
}

// 先頭～2KB から encoding/charset 宣言を拾う
static QByteArray findDeclaredEncoding(const QByteArray& head)
{
    QByteArray lower = head.left(2048).toLower();
    int pos = lower.indexOf("encoding");
    if (pos < 0) pos = lower.indexOf("charset");
    if (pos < 0) return QByteArray();

    int sep = -1;
    int eq = lower.indexOf('=', pos);
    int col = lower.indexOf(':', pos);
    if (eq >= 0 && col >= 0) sep = qMin(eq, col);
    else if (eq >= 0) sep = eq;
    else sep = col;

    if (sep < 0) return QByteArray();

    int i = sep + 1;
    while (i < lower.size() && isspace(uchar(lower[i]))) ++i;
    int j = i;
    while (j < lower.size()
           && !isspace(uchar(lower[j]))
           && lower[j] != '\r' && lower[j] != '\n') ++j;

    QByteArray enc = lower.mid(i, j - i).trimmed();
    enc.replace('"', "").replace('\'', "");
    return enc; // 小文字で返す
}

// 宣言名に対する同義語セットを返す（優先順で並べる）
static QVector<QByteArray> sjisSynonyms(const QByteArray& originalLower)
{
    QVector<QByteArray> list;
    if (originalLower == "shift_jis") list << "Shift_JIS";
    else if (originalLower == "shift-jis") list << "Shift-JIS";
    else if (originalLower == "sjis") list << "SJIS";
    else if (originalLower == "cp932") list << "CP932";
    else if (originalLower == "ms932") list << "MS932";
    else if (originalLower == "windows-31j") list << "Windows-31J";
    else list << originalLower;
    list << "Shift_JIS" << "Shift-JIS" << "CP932" << "Windows-31J" << "MS932" << "SJIS" << "MS_Kanji";
    return list;
}

static QVector<QByteArray> declaredSynonyms(const QByteArray& declLower)
{
    if (declLower == "shift_jis" || declLower == "shift-jis"
        || declLower == "sjis" || declLower == "cp932"
        || declLower == "ms932" || declLower == "windows-31j") {
        return sjisSynonyms(declLower);
    }
    if (declLower == "euc-jp" || declLower == "eucjp") {
        return {"EUC-JP"};
    }
    if (declLower == "iso-2022-jp" || declLower == "jis") {
        return {"ISO-2022-JP"};
    }
    if (declLower == "utf8" || declLower == "utf-8") {
        return {"UTF-8"};
    }
    if (declLower == "utf-16le") return {"UTF-16LE"};
    if (declLower == "utf-16be") return {"UTF-16BE"};
    return {declLower};
}

// 候補の並列試行（最初に成功したものを採用）
static bool tryNameList(const QByteArray& bytes,
                        const QVector<QByteArray>& names,
                        QString& out, QString* usedEncoding)
{
    for (const QByteArray& name : names) {
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
        if (decodeWithName_Qt6(bytes, name.constData(), out)) {
#else
        if (decodeWithName_Qt5(bytes, name.constData(), out)) {
#endif
            if (usedEncoding) *usedEncoding = QString::fromLatin1(name).toLower();
            return true;
        }
    }
    return false;
}

// CR/LF混在を吸収した分割（clazy: static化）
static QStringList splitToLines(const QString& text)
{
    static const QRegularExpression s_lineBreakRe(QStringLiteral("\r\n|\n|\r"));
    return text.split(s_lineBreakRe, Qt::KeepEmptyParts);
}

} // namespace

// ===================== 公開 API =====================

bool KifReader::readTextAuto(const QString& path,
                             QString& outText,
                             QString* usedEncoding,
                             QString* error)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (error) *error += QStringLiteral("[open fail] %1\n").arg(path);
        return false;
    }
    const QByteArray bytes = f.readAll();

    // 1) BOM
    if (decodeWithBom(bytes, outText, usedEncoding)) return true;

    // 2) encoding/charset 宣言の尊重（同義語も総当り）
    const QByteArray declLower = findDeclaredEncoding(bytes);
    if (!declLower.isEmpty()) {
        const QVector<QByteArray> names = declaredSynonyms(declLower);
        if (tryNameList(bytes, names, outText, usedEncoding)) return true;

        // ★ 特別扱い：Shift_JIS 系の宣言だが Qt で失敗した場合、WinAPI/iconv で再トライ
        if (declLower == "shift_jis" || declLower == "shift-jis"
            || declLower == "sjis" || declLower == "cp932"
            || declLower == "ms932" || declLower == "windows-31j") {
            if (decodeShiftJIS_Win(bytes, outText)) {
                if (usedEncoding) *usedEncoding = QStringLiteral("cp932(winapi)");
                return true;
            }
#ifndef Q_OS_WIN
            if (decodeShiftJIS_Iconv(bytes, outText)) {
                if (usedEncoding) *usedEncoding = QStringLiteral("cp932(iconv)");
                return true;
            }
#endif
        }
    }

    // 3) 既知候補（優先順）
    static const QVector<QByteArray> kCandidates = {
        "UTF-8",
        // Shift-JIS 群
        "Shift_JIS", "Shift-JIS", "CP932", "Windows-31J", "MS932", "SJIS", "MS_Kanji",
        // 他の和文系
        "EUC-JP", "ISO-2022-JP"
    };
    if (tryNameList(bytes, kCandidates, outText, usedEncoding)) return true;

    // 3.5) Shift-JIS の保険（宣言が無いファイルにも効く）
    if (decodeShiftJIS_Win(bytes, outText)) {
        if (usedEncoding) *usedEncoding = QStringLiteral("cp932(winapi?)");
        return true;
    }
#ifndef Q_OS_WIN
    if (decodeShiftJIS_Iconv(bytes, outText)) {
        if (usedEncoding) *usedEncoding = QStringLiteral("cp932(iconv?)");
        return true;
    }
#endif

    // 4) 保険: UTF-8 とみなして返す（警告付き）
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
    (void)decodeWithName_Qt6(bytes, "UTF-8", outText);
#else
    (void)decodeWithName_Qt5(bytes, "UTF-8", outText);
#endif
    if (usedEncoding) *usedEncoding = QStringLiteral("utf-8(?)");
    if (error) *error += QStringLiteral("[warn] encoding auto-detect failed, treated as UTF-8\n");
    return true;
}

bool KifReader::readLinesAuto(const QString& path,
                              QStringList& outLines,
                              QString* usedEncoding,
                              QString* error)
{
    QString text;
    if (!readTextAuto(path, text, usedEncoding, error)) return false;
    outLines = splitToLines(text);
    return true;
}
