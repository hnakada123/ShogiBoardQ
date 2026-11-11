#ifndef P2PSIGNATURE_H
#define P2PSIGNATURE_H

/// @file p2psignature.h
/// @brief Ed25519 の鍵・署名・検証と保護付きメモリ（libsodium の包み。実装仕様 5章）

#include <QByteArray>
#include <QByteArrayView>

#include <cstddef>

#include "p2pcbor.h"

namespace P2p {

/**
 * @brief 秘密の値を置く保護付きメモリ（libsodium の sodium_malloc。move-only）
 *
 * 前後に保護ページを置き、スワップへ書き出さないよう OS に求め（応じない OS もある）、解放時に消す。
 * QByteArray にコピーしない。
 */
class SecureBytes
{
public:
    SecureBytes() = default;
    explicit SecureBytes(std::size_t size);
    ~SecureBytes();
    SecureBytes(SecureBytes&& other) noexcept;
    SecureBytes& operator=(SecureBytes&& other) noexcept;
    SecureBytes(const SecureBytes&) = delete;
    SecureBytes& operator=(const SecureBytes&) = delete;

    bool isValid() const { return m_data != nullptr; }
    std::size_t size() const { return m_size; }
    unsigned char* data() { return m_data; }
    const unsigned char* data() const { return m_data; }

private:
    void release();

    unsigned char* m_data = nullptr;
    std::size_t m_size = 0;
};

/**
 * @brief Ed25519 の秘密鍵（プレイヤー鍵・一局用鍵。move-only）
 *
 * 秘密鍵は保護付きメモリに置き、ログ・IPC・証跡・設定へ出さない。公開鍵だけを外へ出す。
 */
class SigningKey
{
public:
    SigningKey() = default;

    /// OS の CSPRNG から作る（libsodium を使えなければ無効な鍵）
    static SigningKey generate();
    /// 32バイトの種から作る（保管した鍵の読み込み）
    static SigningKey fromSeed(const SecureBytes& seed);

    bool isValid() const { return m_secret.isValid(); }
    /// 公開鍵（32バイト）
    QByteArray publicKey() const { return m_publicKey; }
    /// 種（32バイト）を保護付きメモリに写す（保管するときだけ使う）
    SecureBytes seed() const;
    /// メッセージへの署名（64バイト）
    QByteArray sign(QByteArrayView message) const;
    /// 値の決定的CBORへの署名
    QByteArray sign(const CborValue& value) const { return sign(Cbor::encode(value)); }

private:
    SecureBytes m_secret;        ///< crypto_sign の秘密鍵（64バイト）
    QByteArray m_publicKey;
};

namespace Crypto {

/// libsodium を初期化する（何度呼んでもよい）。使えなければ false
bool initialize();

/**
 * @brief Ed25519 の署名を確かめる
 *
 * 公開鍵が正しい点（位数の小さい点でない）であること、署名の S が正規の形であること、R が位数の小さい点で
 * ないことも確かめる（実装仕様 5.5節。libsodium の検証と、公開鍵の点の検査を合わせる）。
 */
bool verify(const QByteArray& publicKey, QByteArrayView message, const QByteArray& signature);
inline bool verify(const QByteArray& publicKey, const CborValue& value, const QByteArray& signature)
{
    return verify(publicKey, Cbor::encode(value), signature);
}

/// 公開鍵として使える点か（32バイト・正規の形・主部分群の点）
bool isValidPublicKey(const QByteArray& publicKey);

/// OS の CSPRNG からの乱数
QByteArray randomBytes(int size);

/// 処理時間が内容によらない比較（長さが違えば false）
bool constantTimeEquals(QByteArrayView a, QByteArrayView b);

} // namespace Crypto

} // namespace P2p

#endif // P2PSIGNATURE_H
