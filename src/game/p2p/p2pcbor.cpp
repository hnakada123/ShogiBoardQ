/// @file p2pcbor.cpp
/// @brief P2P対局の決定的CBORの実装

#include "p2pcbor.h"

#include <QCryptographicHash>

#include <algorithm>

namespace P2p {

CborValue CborValue::uint(quint64 value)
{
    CborValue v;
    v.m_type = Type::Unsigned;
    v.m_uint = value;
    return v;
}

CborValue CborValue::bytes(const QByteArray& value)
{
    CborValue v;
    v.m_type = Type::Bytes;
    v.m_data = value;
    return v;
}

CborValue CborValue::text(const QByteArray& utf8)
{
    Q_ASSERT(Cbor::isValidUtf8(utf8));
    CborValue v;
    v.m_type = Type::Text;
    v.m_data = utf8;
    return v;
}

CborValue CborValue::array(std::vector<CborValue> items)
{
    CborValue v;
    v.m_type = Type::Array;
    v.m_items = std::move(items);
    return v;
}

bool operator==(const CborValue& a, const CborValue& b)
{
    if (a.m_type != b.m_type) return false;
    switch (a.m_type) {
    case CborValue::Type::Unsigned: return a.m_uint == b.m_uint;
    case CborValue::Type::Bytes:
    case CborValue::Type::Text: return a.m_data == b.m_data;
    case CborValue::Type::Array: return a.m_items == b.m_items;
    }
    return false;
}

namespace Cbor {

namespace {

constexpr quint8 kMajorUnsigned = 0;
constexpr quint8 kMajorBytes = 2;
constexpr quint8 kMajorText = 3;
constexpr quint8 kMajorArray = 4;

void appendHead(QByteArray& out, quint8 major, quint64 argument)
{
    const auto initial = [major](quint8 info) { return static_cast<char>((major << 5) | info); };
    if (argument < 24) {
        out.append(initial(static_cast<quint8>(argument)));
    } else if (argument <= 0xFFu) {
        out.append(initial(24));
        out.append(static_cast<char>(argument));
    } else if (argument <= 0xFFFFu) {
        out.append(initial(25));
        for (int shift = 8; shift >= 0; shift -= 8) out.append(static_cast<char>((argument >> shift) & 0xFFu));
    } else if (argument <= 0xFFFFFFFFu) {
        out.append(initial(26));
        for (int shift = 24; shift >= 0; shift -= 8) out.append(static_cast<char>((argument >> shift) & 0xFFu));
    } else {
        out.append(initial(27));
        for (int shift = 56; shift >= 0; shift -= 8) out.append(static_cast<char>((argument >> shift) & 0xFFu));
    }
}

void encodeInto(QByteArray& out, const CborValue& value)
{
    switch (value.type()) {
    case CborValue::Type::Unsigned:
        appendHead(out, kMajorUnsigned, value.toUInt());
        break;
    case CborValue::Type::Bytes:
    case CborValue::Type::Text:
        appendHead(out, value.isBytes() ? kMajorBytes : kMajorText, static_cast<quint64>(value.data().size()));
        out.append(value.data());
        break;
    case CborValue::Type::Array:
        appendHead(out, kMajorArray, static_cast<quint64>(value.items().size()));
        for (const CborValue& item : value.items()) encodeInto(out, item);
        break;
    }
}

/// 外から来たバイト列を読む（上限・最短の形・UTF-8 を確かめる）
class Decoder
{
public:
    explicit Decoder(QByteArrayView bytes) : m_bytes(bytes) {}

    std::optional<CborValue> readValue(int depth)
    {
        if (depth > kMaxDepth) return fail(QStringLiteral("nesting too deep"));
        quint8 major = 0;
        quint64 argument = 0;
        if (!readHead(major, argument)) return std::nullopt;
        switch (major) {
        case kMajorUnsigned:
            return CborValue::uint(argument);
        case kMajorBytes:
        case kMajorText: {
            if (argument > static_cast<quint64>(remaining())) return fail(QStringLiteral("string exceeds input"));
            const qsizetype length = static_cast<qsizetype>(argument);
            const QByteArray data = m_bytes.sliced(m_pos, length).toByteArray();
            m_pos += length;
            if (major == kMajorBytes) return CborValue::bytes(data);
            if (!isValidUtf8(data)) return fail(QStringLiteral("invalid UTF-8"));
            return CborValue::text(data);
        }
        case kMajorArray: {
            // 要素はどれも1バイト以上なので、残りより多い要素数は読めない（先に確かめてから確保する）
            if (argument > static_cast<quint64>(remaining())) return fail(QStringLiteral("array exceeds input"));
            std::vector<CborValue> items;
            items.reserve(static_cast<std::size_t>(argument));
            for (quint64 i = 0; i < argument; ++i) {
                std::optional<CborValue> item = readValue(depth + 1);
                if (!item) return std::nullopt;
                items.push_back(std::move(*item));
            }
            return CborValue::array(std::move(items));
        }
        default:
            return fail(QStringLiteral("unsupported major type"));
        }
    }

    bool atEnd() const { return m_pos == m_bytes.size(); }
    const QString& error() const { return m_error; }

private:
    qsizetype remaining() const { return m_bytes.size() - m_pos; }

    std::nullopt_t fail(const QString& reason)
    {
        if (m_error.isEmpty()) m_error = reason;
        return std::nullopt;
    }

    bool readHead(quint8& major, quint64& argument)
    {
        if (remaining() < 1) {
            fail(QStringLiteral("truncated"));
            return false;
        }
        const auto initial = static_cast<quint8>(m_bytes.at(m_pos++));
        major = static_cast<quint8>(initial >> 5);
        const quint8 info = initial & 0x1Fu;
        if (info < 24) {
            argument = info;
            return true;
        }
        int size = 0;
        quint64 minimum = 0;
        switch (info) {
        case 24: size = 1; minimum = 24; break;
        case 25: size = 2; minimum = 0x100; break;
        case 26: size = 4; minimum = 0x10000; break;
        case 27: size = 8; minimum = 0x100000000ULL; break;
        default:
            fail(QStringLiteral("indefinite or reserved length"));
            return false;
        }
        if (remaining() < size) {
            fail(QStringLiteral("truncated"));
            return false;
        }
        argument = 0;
        for (int i = 0; i < size; ++i) argument = (argument << 8) | static_cast<quint8>(m_bytes.at(m_pos++));
        if (argument < minimum) {
            fail(QStringLiteral("non-shortest encoding"));
            return false;
        }
        return true;
    }

    QByteArrayView m_bytes;
    qsizetype m_pos = 0;
    QString m_error;
};

} // namespace

QByteArray encode(const CborValue& value)
{
    QByteArray out;
    encodeInto(out, value);
    return out;
}

QByteArray sha256(QByteArrayView bytes)
{
    return QCryptographicHash::hash(bytes, QCryptographicHash::Sha256);
}

QByteArray hash(const CborValue& value)
{
    return sha256(encode(value));
}

std::optional<CborValue> decode(QByteArrayView bytes, QString* error)
{
    Decoder decoder(bytes);
    std::optional<CborValue> value = decoder.readValue(1);
    if (value && !decoder.atEnd()) {
        if (error) *error = QStringLiteral("trailing bytes");
        return std::nullopt;
    }
    if (!value) {
        if (error) *error = decoder.error();
        return std::nullopt;
    }
    return value;
}

bool isValidUtf8(QByteArrayView bytes)
{
    qsizetype i = 0;
    const qsizetype n = bytes.size();
    while (i < n) {
        const auto b0 = static_cast<quint8>(bytes.at(i));
        if (b0 < 0x80) {
            ++i;
            continue;
        }
        int length = 0;
        quint32 minimum = 0;
        quint32 codePoint = 0;
        if ((b0 & 0xE0u) == 0xC0u) {
            length = 2;
            minimum = 0x80;
            codePoint = b0 & 0x1Fu;
        } else if ((b0 & 0xF0u) == 0xE0u) {
            length = 3;
            minimum = 0x800;
            codePoint = b0 & 0x0Fu;
        } else if ((b0 & 0xF8u) == 0xF0u) {
            length = 4;
            minimum = 0x10000;
            codePoint = b0 & 0x07u;
        } else {
            return false;
        }
        if (n - i < length) return false;
        for (int k = 1; k < length; ++k) {
            const auto b = static_cast<quint8>(bytes.at(i + k));
            if ((b & 0xC0u) != 0x80u) return false;
            codePoint = (codePoint << 6) | (b & 0x3Fu);
        }
        if (codePoint < minimum || codePoint > 0x10FFFFu) return false;
        if (codePoint >= 0xD800u && codePoint <= 0xDFFFu) return false;
        i += length;
    }
    return true;
}

} // namespace Cbor

namespace {

const CborValue& emptyValue()
{
    static const CborValue value;
    return value;
}

const std::vector<CborValue>& emptyItems()
{
    static const std::vector<CborValue> items;
    return items;
}

bool isPrintableAscii(const QByteArray& bytes)
{
    return std::all_of(bytes.cbegin(), bytes.cend(), [](char c) { return c >= 0x20 && c <= 0x7E; });
}

} // namespace

// ---------------------------------------------------------------- CborReader

CborReader::CborReader(const CborValue& value, const char* label, std::size_t count)
    : m_value(value)
{
    if (!value.isArray() || value.items().size() != count) {
        m_ok = false;
        return;
    }
    if (label) {
        if (count < 2 || !expectText(0, label) || uint(1, 1) != 1) m_ok = false;
    }
}

const CborValue* CborReader::at(std::size_t index)
{
    if (!m_ok || !m_value.isArray() || index >= m_value.items().size()) {
        m_ok = false;
        return nullptr;
    }
    return &m_value.items()[index];
}

quint64 CborReader::uint(std::size_t index, quint64 maximum)
{
    const CborValue* v = at(index);
    if (!v || !v->isUInt() || v->toUInt() > maximum) {
        m_ok = false;
        return 0;
    }
    return v->toUInt();
}

QByteArray CborReader::bytes(std::size_t index, int exactLength)
{
    const CborValue* v = at(index);
    if (!v || !v->isBytes() || v->data().size() != exactLength) {
        m_ok = false;
        return {};
    }
    return v->data();
}

QByteArray CborReader::bytesUpTo(std::size_t index, int maxLength)
{
    const CborValue* v = at(index);
    if (!v || !v->isBytes() || v->data().size() > maxLength) {
        m_ok = false;
        return {};
    }
    return v->data();
}

QByteArray CborReader::ascii(std::size_t index, int maxLength)
{
    const CborValue* v = at(index);
    if (!v || !v->isText() || v->data().size() > maxLength || !isPrintableAscii(v->data())) {
        m_ok = false;
        return {};
    }
    return v->data();
}

QString CborReader::utf8(std::size_t index, int maxBytes)
{
    const CborValue* v = at(index);
    if (!v || !v->isText() || v->data().size() > maxBytes) {
        m_ok = false;
        return {};
    }
    return v->toText();
}

const std::vector<CborValue>& CborReader::array(std::size_t index, std::size_t maxCount)
{
    const CborValue* v = at(index);
    if (!v || !v->isArray() || v->items().size() > maxCount) {
        m_ok = false;
        return emptyItems();
    }
    return v->items();
}

const CborValue& CborReader::item(std::size_t index)
{
    const CborValue* v = at(index);
    return v ? *v : emptyValue();
}

bool CborReader::expectText(std::size_t index, const char* label)
{
    const CborValue* v = at(index);
    if (!v || !v->isText() || v->data() != QByteArray(label)) {
        m_ok = false;
        return false;
    }
    return true;
}

} // namespace P2p
