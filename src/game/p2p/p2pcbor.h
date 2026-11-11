#ifndef P2PCBOR_H
#define P2PCBOR_H

/// @file p2pcbor.h
/// @brief P2P対局の決定的CBOR（符号なし整数・byte string・text string・配列だけ。protocol/README.md 1章）

#include <QByteArray>
#include <QByteArrayView>
#include <QString>

#include <optional>
#include <vector>

namespace P2p {

/**
 * @brief 決定的CBORの値（4つの型だけ）
 *
 * 署名・ハッシュの対象と対局フレームは、すべてこの値の配列で表す。符号化は1通りしかないので、
 * 同じ値からは同じバイト列ができる。外から来たバイト列は Cbor::decode だけで読む。
 */
class CborValue
{
public:
    enum class Type { Unsigned, Bytes, Text, Array };

    CborValue() = default;

    static CborValue uint(quint64 value);
    static CborValue bytes(const QByteArray& value);
    /// ASCII または UTF-8 の text string（正しい UTF-8 でなければ呼ぶ側の誤り）
    static CborValue text(const QByteArray& utf8);
    static CborValue text(const QString& value) { return text(value.toUtf8()); }
    static CborValue text(const char* ascii) { return text(QByteArray(ascii)); }
    static CborValue array(std::vector<CborValue> items);

    Type type() const { return m_type; }
    bool isUInt() const { return m_type == Type::Unsigned; }
    bool isBytes() const { return m_type == Type::Bytes; }
    bool isText() const { return m_type == Type::Text; }
    bool isArray() const { return m_type == Type::Array; }

    quint64 toUInt() const { return m_uint; }
    /// byte string の中身、または text string の UTF-8
    const QByteArray& data() const { return m_data; }
    /// text string を QString にする（先頭の BOM は QString::fromUtf8 が取り除くので、判定には data() を使う）
    QString toText() const { return QString::fromUtf8(m_data); }
    const std::vector<CborValue>& items() const { return m_items; }

    friend bool operator==(const CborValue& a, const CborValue& b);
    friend bool operator!=(const CborValue& a, const CborValue& b) { return !(a == b); }

private:
    Type m_type = Type::Unsigned;
    quint64 m_uint = 0;
    QByteArray m_data;
    std::vector<CborValue> m_items;
};

namespace Cbor {

/// 入れ子の上限（最上位の値を1段目と数える）
inline constexpr int kMaxDepth = 16;

/// 決定的CBORに符号化する
QByteArray encode(const CborValue& value);

/// SHA-256(encode(value))（32バイト）
QByteArray hash(const CborValue& value);

/// SHA-256（32バイト）
QByteArray sha256(QByteArrayView bytes);

/**
 * @brief 決定的CBORのバイト列を読む
 *
 * 4つの型・最短の長さ・確定長・正しい UTF-8・入れ子の上限・余りのバイトがないことを確かめ、
 * 1つでも外れれば nullopt を返す（error に理由）。読み終えた値を符号化し直すと元のバイト列と一致する。
 */
std::optional<CborValue> decode(QByteArrayView bytes, QString* error = nullptr);

/// 正しい UTF-8 か（過長な形・サロゲート・U+10FFFF を超える値を拒否する）
bool isValidUtf8(QByteArrayView bytes);

} // namespace Cbor

} // namespace P2p

#endif // P2PCBOR_H
