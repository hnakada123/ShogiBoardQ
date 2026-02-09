/// @file kifreader.cpp
/// @brief 棋譜ファイルの文字コード自動判別読み込み機能の実装

#include "kifreader.h"

#include <QByteArray>
#include "kifulogging.h"
#include <QFile>
#include <QStringDecoder>

namespace KifReader {

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
    QString s = dec.decode(bytes);
    ok = !s.contains(QChar(0xFFFD));
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
    if (sz >= 2 &&
        data[0] == 0xFF &&
        data[1] == 0xFE) {
        // UTF-16LE
        QStringDecoder dec("UTF-16LE");
        out = dec.decode(bytes.mid(2));
        if (usedEncoding) *usedEncoding = QStringLiteral("utf-16le(bom)");
        return true;
    }
    if (sz >= 2 &&
        data[0] == 0xFE &&
        data[1] == 0xFF) {
        // UTF-16BE
        QStringDecoder dec("UTF-16BE");
        out = dec.decode(bytes.mid(2));
        if (usedEncoding) *usedEncoding = QStringLiteral("utf-16be(bom)");
        return true;
    }
    if (sz >= 4 &&
        data[0] == 0xFF &&
        data[1] == 0xFE &&
        data[2] == 0x00 &&
        data[3] == 0x00) {
        // UTF-32LE
        QStringDecoder dec("UTF-32LE");
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
        QStringDecoder dec("UTF-32BE");
        out = dec.decode(bytes.mid(4));
        if (usedEncoding) *usedEncoding = QStringLiteral("utf-32be(bom)");
        return true;
    }
    return false;
}

// --- public API ------------------------------------------------------------

bool readAllLinesAuto(const QString& filePath,
                      QStringList& outLines,
                      QString* usedEncoding,
                      QString* warn)
{
    outLines.clear();

    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly)) {
        if (warn) *warn += QStringLiteral("open failed: %1\n").arg(filePath);
        return false;
    }
    const QByteArray bytes = f.readAll();

    QString text;
    // 1) BOM 優先
    if (!decodeByBom(bytes, text, usedEncoding)) {
        // 2) UTF-8 を試す（置換文字の有無で判定）
        bool okUtf8 = false;
        QString sUtf8 = decodeWith(bytes, "UTF-8", okUtf8);
        if (okUtf8) {
            text = std::move(sUtf8);
            if (usedEncoding) *usedEncoding = QStringLiteral("utf-8");
        } else {
            // 3) CP932 / Shift_JIS
            bool okSj = false;
            QString sSj = decodeWith(bytes, "Shift-JIS", okSj);
            if (okSj) {
                text = std::move(sSj);
                if (usedEncoding) *usedEncoding = QStringLiteral("cp932(iconv)");
            } else {
                // 4) EUC-JP（稀）
                bool okEuc = false;
                QString sEuc = decodeWith(bytes, "EUC-JP", okEuc);
                if (okEuc) {
                    text = std::move(sEuc);
                    if (usedEncoding) *usedEncoding = QStringLiteral("euc-jp");
                } else {
                    // 5) 仕方なくローカル 8bit
                    text = QString::fromLocal8Bit(bytes);
                    if (usedEncoding) *usedEncoding = QStringLiteral("local8bit(fallback)");
                    if (warn) *warn += QStringLiteral("fallback: local8bit was used; encoding could not be detected precisely.\n");
                }
            }
        }
    }

    if (usedEncoding) {
        qCDebug(lcKifu).noquote() << QStringLiteral("readAllLinesAuto: encoding = %1, bytes = %2")
                                          .arg(*usedEncoding)
                                          .arg(bytes.size());
    }

    splitByNewlines(text, outLines);
    return true;
}

} // namespace KifReader
