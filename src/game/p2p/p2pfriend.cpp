/// @file p2pfriend.cpp
/// @brief 友だち一覧の在席の知らせの実装

#include "p2pfriend.h"

#include <sodium.h>

#include "p2pcbor.h"
#include "p2pprotocol.h"
#include "p2psignature.h"

namespace P2p::Friend {

namespace {

constexpr int kSecretSize = 32;
/// 知らせの中身（通信プロセスが作る招待の本体と同じ形）の上限
constexpr int kMaxBodySize = kMaxBlobSize;

QByteArray labeled(const char* label, const QByteArray& networkId, const QByteArray& pairSecret,
                   const QByteArray& depositor, quint64 epoch)
{
    if (networkId.size() != kHashSize || pairSecret.size() != kSecretSize || depositor.size() != kPublicKeySize) return {};
    return Cbor::hash(CborValue::array({CborValue::text(label), CborValue::uint(1), CborValue::bytes(networkId),
                                        CborValue::bytes(pairSecret), CborValue::bytes(depositor),
                                        CborValue::uint(epoch)}));
}

/// 暗号化で一緒に認証する値（ネットワーク・預ける側・期間。別の期間・別の人の知らせを読まない）
QByteArray associatedData(const QByteArray& networkId, const QByteArray& depositor, quint64 epoch)
{
    return Cbor::encode(CborValue::array({CborValue::text("SBQ-FRIEND-BLOB"), CborValue::uint(1),
                                          CborValue::bytes(networkId), CborValue::bytes(depositor),
                                          CborValue::uint(epoch)}));
}

const unsigned char* raw(const QByteArray& bytes)
{
    return reinterpret_cast<const unsigned char*>(bytes.constData());
}

} // namespace

QByteArray pairSecretFromShared(const QByteArray& shared, const QByteArray& playerKeyA, const QByteArray& playerKeyB)
{
    if (shared.size() != kSecretSize || playerKeyA.size() != kPublicKeySize || playerKeyB.size() != kPublicKeySize
        || playerKeyA == playerKeyB) {
        return {};
    }
    const bool aFirst = playerKeyA < playerKeyB;
    return Cbor::hash(CborValue::array({CborValue::text("SBQ-FRIEND-PAIR"), CborValue::uint(1), CborValue::bytes(shared),
                                        CborValue::bytes(aFirst ? playerKeyA : playerKeyB),
                                        CborValue::bytes(aFirst ? playerKeyB : playerKeyA)}));
}

QByteArray pairSecret(const SigningKey& mine, const QByteArray& theirPlayerKey)
{
    const SecureBytes shared = mine.sharedSecret(theirPlayerKey);
    if (!shared.isValid()) return {};
    // ハッシュに通すための写し。使い終えたら消す
    QByteArray copy(reinterpret_cast<const char*>(shared.data()), static_cast<qsizetype>(shared.size()));
    const QByteArray secret = pairSecretFromShared(copy, mine.publicKey(), theirPlayerKey);
    sodium_memzero(copy.data(), static_cast<std::size_t>(copy.size()));
    return secret;
}

QByteArray slot(const QByteArray& networkId, const QByteArray& pairSecret, const QByteArray& depositor, quint64 epoch)
{
    return labeled("SBQ-FRIEND-SLOT", networkId, pairSecret, depositor, epoch);
}

QByteArray key(const QByteArray& networkId, const QByteArray& pairSecret, const QByteArray& depositor, quint64 epoch)
{
    return labeled("SBQ-FRIEND-KEY", networkId, pairSecret, depositor, epoch);
}

QByteArray seal(const QByteArray& networkId, const QByteArray& pairSecret, const QByteArray& depositor, quint64 epoch,
                const Presence& presence, const QByteArray& nonce)
{
    if (!Crypto::initialize() || presence.body.isEmpty() || presence.body.size() > kMaxBodySize) return {};
    const QByteArray secretKey = key(networkId, pairSecret, depositor, epoch);
    if (secretKey.isEmpty()) return {};
    const QByteArray usedNonce = nonce.isEmpty() ? Crypto::randomBytes(kNonceSize) : nonce;
    if (usedNonce.size() != kNonceSize) return {};
    const QByteArray plain = Cbor::encode(CborValue::array({CborValue::text("SBQ-PRESENCE"), CborValue::uint(1),
                                                            CborValue::uint(presence.expiresAt),
                                                            CborValue::bytes(presence.body)}));
    const QByteArray aad = associatedData(networkId, depositor, epoch);
    QByteArray sealed(plain.size() + kTagSize, '\0');
    unsigned long long sealedSize = 0;
    if (crypto_aead_xchacha20poly1305_ietf_encrypt(reinterpret_cast<unsigned char*>(sealed.data()), &sealedSize,
                                                   raw(plain), static_cast<unsigned long long>(plain.size()), raw(aad),
                                                   static_cast<unsigned long long>(aad.size()), nullptr, raw(usedNonce),
                                                   raw(secretKey))
        != 0) {
        return {};
    }
    sealed.truncate(static_cast<qsizetype>(sealedSize));
    const QByteArray blob = usedNonce + sealed;
    return blob.size() > kMaxBlobSize ? QByteArray() : blob;
}

std::optional<Presence> open(const QByteArray& networkId, const QByteArray& pairSecret, const QByteArray& depositor,
                             quint64 epoch, const QByteArray& blob)
{
    if (!Crypto::initialize() || blob.size() < kNonceSize + kTagSize || blob.size() > kMaxBlobSize) return std::nullopt;
    const QByteArray secretKey = key(networkId, pairSecret, depositor, epoch);
    if (secretKey.isEmpty()) return std::nullopt;
    const QByteArray nonce = blob.left(kNonceSize);
    const QByteArray sealed = blob.mid(kNonceSize);
    const QByteArray aad = associatedData(networkId, depositor, epoch);
    QByteArray plain(sealed.size() - kTagSize, '\0');
    unsigned long long plainSize = 0;
    // 認証の検査に通らないもの（鍵を知らない協力ノードや第三者が作ったもの）は、ここで捨てる
    if (crypto_aead_xchacha20poly1305_ietf_decrypt(reinterpret_cast<unsigned char*>(plain.data()), &plainSize, nullptr,
                                                   raw(sealed), static_cast<unsigned long long>(sealed.size()), raw(aad),
                                                   static_cast<unsigned long long>(aad.size()), raw(nonce), raw(secretKey))
        != 0) {
        return std::nullopt;
    }
    plain.truncate(static_cast<qsizetype>(plainSize));
    const std::optional<CborValue> value = Cbor::decode(plain);
    if (!value) return std::nullopt;
    CborReader reader(*value, "SBQ-PRESENCE", 4);
    Presence presence;
    presence.expiresAt = reader.uint(2, ~quint64(0));
    presence.body = reader.bytesUpTo(3, kMaxBodySize);
    if (!reader.ok() || presence.body.isEmpty()) return std::nullopt;
    return presence;
}

} // namespace P2p::Friend
