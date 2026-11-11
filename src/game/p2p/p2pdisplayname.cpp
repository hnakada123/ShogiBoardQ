/// @file p2pdisplayname.cpp
/// @brief P2P対局の表示名の規則の実装

#include "p2pdisplayname.h"

#include <QByteArray>
#include <QList>

#include <utility>

#include "p2pcbor.h"
#include "p2pdisplaynametable.h"   // CMake が protocol/displayname-v1.txt から作る（kDisplayNameTableV1）

namespace P2p {

namespace DisplayName {

namespace {

using Range = std::pair<char32_t, char32_t>;

/// 表の行 "XXXX..YYYY" を読む（# の行と空行は読み飛ばす。読めない行は表の誤りなので範囲に入れない）
QList<Range> parseTable()
{
    QList<Range> ranges;
    const QList<QByteArray> lines = QByteArray(kDisplayNameTableV1).split('\n');
    for (QByteArray line : lines) {
        line = line.trimmed();
        if (line.isEmpty() || line.startsWith('#')) continue;
        const qsizetype dots = line.indexOf("..");
        if (dots <= 0) continue;
        bool okFirst = false;
        bool okLast = false;
        const uint first = line.left(dots).toUInt(&okFirst, 16);
        const uint last = line.mid(dots + 2).toUInt(&okLast, 16);
        if (!okFirst || !okLast || first > last || last > 0x10FFFFu) continue;
        ranges.append({static_cast<char32_t>(first), static_cast<char32_t>(last)});
    }
    return ranges;
}

const QList<Range>& table()
{
    static const QList<Range> ranges = parseTable();
    return ranges;
}

} // namespace

bool isAllowedCodePoint(char32_t codePoint)
{
    for (const Range& range : table()) {
        if (codePoint >= range.first && codePoint <= range.second) return true;
    }
    return false;
}

bool isValidUtf8(QByteArrayView utf8)
{
    if (utf8.isEmpty() || utf8.size() > kMaxBytes || !Cbor::isValidUtf8(utf8)) return false;
    // QString::fromUtf8 は先頭の BOM を黙って取り除くので使わず、バイト列から直接コードポイントにする
    QList<char32_t> codePoints;
    for (qsizetype i = 0; i < utf8.size();) {
        const auto lead = static_cast<quint8>(utf8.at(i));
        const int length = lead < 0x80 ? 1 : (lead < 0xE0 ? 2 : (lead < 0xF0 ? 3 : 4));
        char32_t cp = length == 1 ? lead : (lead & (0xFFu >> (length + 1)));
        for (int k = 1; k < length; ++k) cp = (cp << 6) | (static_cast<quint8>(utf8.at(i + k)) & 0x3Fu);
        codePoints.append(cp);
        i += length;
    }
    if (codePoints.isEmpty() || codePoints.size() > kMaxCodePoints) return false;
    if (codePoints.first() == 0x20u || codePoints.last() == 0x20u) return false;
    for (const char32_t cp : std::as_const(codePoints)) {
        if (!isAllowedCodePoint(cp)) return false;
    }
    return true;
}

bool isValid(const QString& name)
{
    return isValidUtf8(name.toUtf8());
}

QString comparisonKey(const QString& name)
{
    QString key;
    key.reserve(name.size());
    for (const QChar ch : name) {
        char16_t c = ch.unicode();
        if (c >= 0xFF01 && c <= 0xFF5E) c = static_cast<char16_t>(c - 0xFF01 + 0x21);
        if (c >= u'A' && c <= u'Z') c = static_cast<char16_t>(c - u'A' + u'a');
        key.append(QChar(c));
    }
    return key;
}

} // namespace DisplayName

} // namespace P2p
