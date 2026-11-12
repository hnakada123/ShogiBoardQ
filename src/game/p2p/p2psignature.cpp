/// @file p2psignature.cpp
/// @brief Ed25519 の鍵・署名・検証と保護付きメモリの実装

#include "p2psignature.h"

#include <sodium.h>

#include <cstring>
#include <utility>

namespace P2p {

// ---------------------------------------------------------------- SecureBytes

SecureBytes::SecureBytes(std::size_t size)
{
    if (!Crypto::initialize() || size == 0) return;
    m_data = static_cast<unsigned char*>(sodium_malloc(size));
    if (m_data) m_size = size;
}

SecureBytes::~SecureBytes()
{
    release();
}

SecureBytes::SecureBytes(SecureBytes&& other) noexcept
    : m_data(std::exchange(other.m_data, nullptr))
    , m_size(std::exchange(other.m_size, 0))
{
}

SecureBytes& SecureBytes::operator=(SecureBytes&& other) noexcept
{
    if (this != &other) {
        release();
        m_data = std::exchange(other.m_data, nullptr);
        m_size = std::exchange(other.m_size, 0);
    }
    return *this;
}

void SecureBytes::release()
{
    // sodium_free は解放の前に中身を消す
    if (m_data) sodium_free(m_data);
    m_data = nullptr;
    m_size = 0;
}

// ---------------------------------------------------------------- SigningKey

SigningKey SigningKey::generate()
{
    SigningKey key;
    SecureBytes secret(crypto_sign_SECRETKEYBYTES);
    if (!secret.isValid()) return key;
    QByteArray publicKey(crypto_sign_PUBLICKEYBYTES, '\0');
    if (crypto_sign_keypair(reinterpret_cast<unsigned char*>(publicKey.data()), secret.data()) != 0) return key;
    key.m_secret = std::move(secret);
    key.m_publicKey = std::move(publicKey);
    return key;
}

SigningKey SigningKey::fromSeed(const SecureBytes& seed)
{
    SigningKey key;
    if (!seed.isValid() || seed.size() != crypto_sign_SEEDBYTES) return key;
    SecureBytes secret(crypto_sign_SECRETKEYBYTES);
    if (!secret.isValid()) return key;
    QByteArray publicKey(crypto_sign_PUBLICKEYBYTES, '\0');
    if (crypto_sign_seed_keypair(reinterpret_cast<unsigned char*>(publicKey.data()), secret.data(), seed.data())
        != 0) {
        return key;
    }
    key.m_secret = std::move(secret);
    key.m_publicKey = std::move(publicKey);
    return key;
}

QByteArray SigningKey::sign(QByteArrayView message) const
{
    if (!isValid()) return {};
    QByteArray signature(crypto_sign_BYTES, '\0');
    if (crypto_sign_detached(reinterpret_cast<unsigned char*>(signature.data()), nullptr,
                             reinterpret_cast<const unsigned char*>(message.data()),
                             static_cast<unsigned long long>(message.size()), m_secret.data())
        != 0) {
        return {};
    }
    return signature;
}

// ---------------------------------------------------------------- Crypto

namespace Crypto {

bool initialize()
{
    static const bool ok = sodium_init() >= 0;
    return ok;
}

bool isValidPublicKey(const QByteArray& publicKey)
{
    if (!initialize() || publicKey.size() != crypto_sign_PUBLICKEYBYTES) return false;
    return crypto_core_ed25519_is_valid_point(reinterpret_cast<const unsigned char*>(publicKey.constData())) == 1;
}

bool verify(const QByteArray& publicKey, QByteArrayView message, const QByteArray& signature)
{
    if (!initialize() || signature.size() != crypto_sign_BYTES || !isValidPublicKey(publicKey)) return false;
    return crypto_sign_verify_detached(reinterpret_cast<const unsigned char*>(signature.constData()),
                                       reinterpret_cast<const unsigned char*>(message.data()),
                                       static_cast<unsigned long long>(message.size()),
                                       reinterpret_cast<const unsigned char*>(publicKey.constData()))
        == 0;
}

QByteArray randomBytes(int size)
{
    if (!initialize() || size <= 0) return {};
    QByteArray out(size, '\0');
    randombytes_buf(out.data(), static_cast<std::size_t>(size));
    return out;
}

} // namespace Crypto

} // namespace P2p
