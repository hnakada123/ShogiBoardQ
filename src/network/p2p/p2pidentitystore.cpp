/// @file p2pidentitystore.cpp
/// @brief プレイヤー鍵の保管の実装

#include "p2pidentitystore.h"

#include <QDir>
#include <QFile>
#include <QFutureWatcher>
#include <QSaveFile>
#include <QtConcurrent/QtConcurrentRun>

#include <sodium.h>

#include "appidentity.h"
#include "p2pcbor.h"
#include "p2pprotocol.h"

#ifdef SBQ_HAVE_QTKEYCHAIN
#include <qt6keychain/keychain.h>
#endif

namespace {

constexpr int kSeedSize = 32;
constexpr int kSaltSize = 16;   // crypto_pwhash_SALTBYTES
constexpr int kNonceSize = 24;  // crypto_aead_xchacha20poly1305_ietf_NPUBBYTES
constexpr int kBoxSize = kSeedSize + 16;
constexpr qint64 kMaxKeyFileBytes = 1024;
/// 鍵の保管庫の項目のキー（サービス名はアプリ名。実験版専用）
const char* const kKeychainKey = "player-key-v1";

/// 暗号化の追加の認証データ CBOR(["SBQ-IDENTITY", 1, 1, opsLimit, memLimitBytes, salt])
QByteArray associatedData(quint64 opsLimit, quint64 memLimitBytes, const QByteArray& salt)
{
    using P2p::CborValue;
    return P2p::Cbor::encode(CborValue::array({CborValue::text("SBQ-IDENTITY"), CborValue::uint(1),
                                               CborValue::uint(1), CborValue::uint(opsLimit),
                                               CborValue::uint(memLimitBytes), CborValue::bytes(salt)}));
}

/// パスフレーズから鍵を導く（Argon2id）
P2p::SecureBytes deriveKey(const QString& passphrase, const QByteArray& salt, quint64 opsLimit, quint64 memLimitBytes)
{
    P2p::SecureBytes key(crypto_aead_xchacha20poly1305_ietf_KEYBYTES);
    if (!key.isValid()) return key;
    QByteArray pass = passphrase.toUtf8();
    const int rc = crypto_pwhash(key.data(), key.size(), pass.constData(), static_cast<unsigned long long>(pass.size()),
                                 reinterpret_cast<const unsigned char*>(salt.constData()), opsLimit,
                                 static_cast<size_t>(memLimitBytes), crypto_pwhash_ALG_ARGON2ID13);
    sodium_memzero(pass.data(), static_cast<size_t>(pass.size()));
    if (rc != 0) return P2p::SecureBytes();
    return key;
}

/// QByteArray にしか置けなかった秘密を消す（できる範囲で）
void wipe(QByteArray& bytes)
{
    if (!bytes.isEmpty()) sodium_memzero(bytes.data(), static_cast<size_t>(bytes.size()));
    bytes.clear();
}

/// 作る処理の結果（ワーカーから値で返す）
struct CreateResult {
    std::shared_ptr<P2p::SecureBytes> seed;
    QByteArray fileBytes;
    P2pIdentityStore::Failure failure = P2pIdentityStore::Failure::None;
};

struct UnlockResult {
    std::shared_ptr<P2p::SecureBytes> seed;
    P2pIdentityStore::Failure failure = P2pIdentityStore::Failure::None;
};

} // namespace

P2pIdentityStore::P2pIdentityStore(const QString& directory, QObject* parent)
    : QObject(parent)
    , m_directory(directory)
{
}

P2pIdentityStore::~P2pIdentityStore() = default;

bool P2pIdentityStore::keychainSupported()
{
#ifdef SBQ_HAVE_QTKEYCHAIN
    return true;
#else
    return false;
#endif
}

QString P2pIdentityStore::infoPath() const
{
    return QDir(m_directory).filePath(QStringLiteral("info"));
}

QString P2pIdentityStore::keyFilePath() const
{
    return QDir(m_directory).filePath(QStringLiteral("player.key"));
}

namespace {

struct Info {
    P2pIdentityStore::Method method = P2pIdentityStore::Method::None;
    QByteArray publicKey;
};

Info readInfo(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    const std::optional<P2p::CborValue> value = P2p::Cbor::decode(file.read(256));
    if (!value) return {};
    P2p::CborReader r(*value, "SBQ-IDENTITY-INFO", 4);
    Info info;
    const quint64 method = r.uint(2, 3);
    info.publicKey = r.bytes(3, P2p::kPublicKeySize);
    if (!r.ok() || method == 0) return {};
    info.method = static_cast<P2pIdentityStore::Method>(method);
    return info;
}

} // namespace

P2pIdentityStore::Method P2pIdentityStore::method() const
{
    return readInfo(infoPath()).method;
}

QByteArray P2pIdentityStore::publicKey() const
{
    if (isUnlocked()) return m_key->publicKey();
    return readInfo(infoPath()).publicKey;
}

bool P2pIdentityStore::writeFileAtomically(const QString& path, const QByteArray& bytes)
{
    if (!QDir().mkpath(m_directory)) return false;
    QFile::setPermissions(m_directory, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return false;
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    if (file.write(bytes) != bytes.size()) {
        file.cancelWriting();
        return false;
    }
    return file.commit();
}

bool P2pIdentityStore::writeInfo(Method method, const QByteArray& publicKey)
{
    using P2p::CborValue;
    const QByteArray bytes = P2p::Cbor::encode(CborValue::array(
        {CborValue::text("SBQ-IDENTITY-INFO"), CborValue::uint(1), CborValue::uint(static_cast<quint64>(method)),
         CborValue::bytes(publicKey)}));
    return writeFileAtomically(infoPath(), bytes);
}

// ---------------------------------------------------------------- 鍵のファイル

QByteArray P2pIdentityStore::encryptFile(const P2p::SecureBytes& seed, const QString& passphrase, quint64 opsLimit,
                                         quint64 memLimitBytes)
{
    using P2p::CborValue;
    if (!P2p::Crypto::initialize() || !seed.isValid() || seed.size() != kSeedSize) return {};
    if (passphrase.isEmpty()) {
        QByteArray raw(reinterpret_cast<const char*>(seed.data()), kSeedSize);
        QByteArray out = P2p::Cbor::encode(CborValue::array(
            {CborValue::text("SBQ-IDENTITY"), CborValue::uint(1), CborValue::uint(0), CborValue::bytes(raw)}));
        wipe(raw);
        return out;
    }
    const QByteArray salt = P2p::Crypto::randomBytes(kSaltSize);
    const QByteArray nonce = P2p::Crypto::randomBytes(kNonceSize);
    const P2p::SecureBytes key = deriveKey(passphrase, salt, opsLimit, memLimitBytes);
    if (!key.isValid()) return {};
    const QByteArray aad = associatedData(opsLimit, memLimitBytes, salt);
    QByteArray box(kBoxSize, '\0');
    unsigned long long boxLength = 0;
    if (crypto_aead_xchacha20poly1305_ietf_encrypt(
            reinterpret_cast<unsigned char*>(box.data()), &boxLength, seed.data(), kSeedSize,
            reinterpret_cast<const unsigned char*>(aad.constData()), static_cast<unsigned long long>(aad.size()),
            nullptr, reinterpret_cast<const unsigned char*>(nonce.constData()), key.data())
            != 0
        || boxLength != static_cast<unsigned long long>(kBoxSize)) {
        return {};
    }
    return P2p::Cbor::encode(CborValue::array({CborValue::text("SBQ-IDENTITY"), CborValue::uint(1), CborValue::uint(1),
                                               CborValue::uint(opsLimit), CborValue::uint(memLimitBytes),
                                               CborValue::bytes(salt), CborValue::bytes(nonce), CborValue::bytes(box)}));
}

std::shared_ptr<P2p::SecureBytes> P2pIdentityStore::decryptFile(const QByteArray& fileBytes, const QString& passphrase,
                                                                Failure* failure)
{
    const auto fail = [failure](Failure f) {
        if (failure) *failure = f;
        return std::shared_ptr<P2p::SecureBytes>();
    };
    if (!P2p::Crypto::initialize()) return fail(Failure::Crypto);
    if (fileBytes.size() > kMaxKeyFileBytes) return fail(Failure::CorruptFile);
    const std::optional<P2p::CborValue> value = P2p::Cbor::decode(fileBytes);
    if (!value || !value->isArray() || value->items().size() < 3) return fail(Failure::CorruptFile);
    const quint64 scheme = value->items()[2].isUInt() ? value->items()[2].toUInt() : 99;
    auto seed = std::make_shared<P2p::SecureBytes>(kSeedSize);
    if (!seed->isValid()) return fail(Failure::Crypto);

    if (scheme == 0) {
        P2p::CborReader r(*value, "SBQ-IDENTITY", 4);
        r.uint(2, 0);
        QByteArray raw = r.bytes(3, kSeedSize);
        if (!r.ok()) return fail(Failure::CorruptFile);
        std::copy(raw.cbegin(), raw.cend(), seed->data());
        wipe(raw);
        if (failure) *failure = Failure::None;
        return seed;
    }

    P2p::CborReader r(*value, "SBQ-IDENTITY", 8);
    r.uint(2, 1);
    const quint64 opsLimit = r.uint(3, 0xFFFFFFFFULL);
    const quint64 memLimit = r.uint(4, 0xFFFFFFFFFFFFULL);
    const QByteArray salt = r.bytes(5, kSaltSize);
    const QByteArray nonce = r.bytes(6, kNonceSize);
    const QByteArray box = r.bytes(7, kBoxSize);
    if (!r.ok()) return fail(Failure::CorruptFile);
    // 範囲外のパラメータは、鍵の導出を始める前に断る
    if (opsLimit < kMinOpsLimit || opsLimit > kMaxOpsLimit || memLimit < kMinMemLimitBytes
        || memLimit > kMaxMemLimitBytes) {
        return fail(Failure::CorruptFile);
    }
    const P2p::SecureBytes key = deriveKey(passphrase, salt, opsLimit, memLimit);
    if (!key.isValid()) return fail(Failure::Crypto);
    const QByteArray aad = associatedData(opsLimit, memLimit, salt);
    unsigned long long seedLength = 0;
    if (crypto_aead_xchacha20poly1305_ietf_decrypt(
            seed->data(), &seedLength, nullptr, reinterpret_cast<const unsigned char*>(box.constData()),
            static_cast<unsigned long long>(box.size()), reinterpret_cast<const unsigned char*>(aad.constData()),
            static_cast<unsigned long long>(aad.size()), reinterpret_cast<const unsigned char*>(nonce.constData()),
            key.data())
            != 0
        || seedLength != static_cast<unsigned long long>(kSeedSize)) {
        return fail(Failure::WrongPassphrase);
    }
    if (failure) *failure = Failure::None;
    return seed;
}

// ---------------------------------------------------------------- 操作

void P2pIdentityStore::finish(Failure failure)
{
    m_busy = false;
    emit finished(failure);
}

void P2pIdentityStore::adoptSeed(const P2p::SecureBytes& seed)
{
    m_key = std::make_unique<P2p::SigningKey>(P2p::SigningKey::fromSeed(seed));
}

void P2pIdentityStore::create(Method method, const QString& passphrase)
{
    if (m_busy) {
        emit finished(Failure::Busy);
        return;
    }
    if (!P2p::Crypto::initialize()) {
        emit finished(Failure::Crypto);
        return;
    }
    m_busy = true;
    lock();
    auto seed = std::make_shared<P2p::SecureBytes>(kSeedSize);
    if (!seed->isValid()) {
        finish(Failure::Crypto);
        return;
    }
    randombytes_buf(seed->data(), kSeedSize);

    if (method == Method::Keychain) {
        storeInKeychain(seed);
        return;
    }
    if (method == Method::Plain) {
        finishCreate(method, seed, encryptFile(*seed, QString(), 0, 0), Failure::None);
        return;
    }
    // パスフレーズからの鍵の導出は時間とメモリを使うので、GUI スレッドの外で行う
    auto* watcher = new QFutureWatcher<CreateResult>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, &P2pIdentityStore::onCreateFinished);
    watcher->setFuture(QtConcurrent::run([seed, passphrase]() {
        CreateResult result;
        result.seed = seed;
        result.fileBytes = encryptFile(*seed, passphrase, kOpsLimit, kMemLimitBytes);
        if (result.fileBytes.isEmpty()) result.failure = Failure::Crypto;
        return result;
    }));
}

void P2pIdentityStore::onCreateFinished()
{
    auto* watcher = static_cast<QFutureWatcher<CreateResult>*>(sender());
    const CreateResult result = watcher->result();
    watcher->deleteLater();
    finishCreate(Method::Passphrase, result.seed, result.fileBytes, result.failure);
}

void P2pIdentityStore::finishCreate(Method method, const std::shared_ptr<P2p::SecureBytes>& seed,
                                    const QByteArray& fileBytes, Failure failure)
{
    if (failure != Failure::None) {
        finish(failure);
        return;
    }
    const P2p::SigningKey key = P2p::SigningKey::fromSeed(*seed);
    if (!key.isValid()) {
        finish(Failure::Crypto);
        return;
    }
    if (method != Method::Keychain) {
        if (!writeFileAtomically(keyFilePath(), fileBytes)) {
            finish(Failure::Storage);
            return;
        }
    } else {
        QFile::remove(keyFilePath());
    }
    if (!writeInfo(method, key.publicKey())) {
        finish(Failure::Storage);
        return;
    }
    adoptSeed(*seed);
    finish(Failure::None);
}

void P2pIdentityStore::unlock(const QString& passphrase)
{
    if (m_busy) {
        emit finished(Failure::Busy);
        return;
    }
    const Info info = readInfo(infoPath());
    if (info.method == Method::None) {
        emit finished(Failure::CorruptFile);
        return;
    }
    m_busy = true;
    if (info.method == Method::Keychain) {
        readFromKeychain();
        return;
    }
    QFile file(keyFilePath());
    if (!file.open(QIODevice::ReadOnly)) {
        finish(Failure::Storage);
        return;
    }
    const QByteArray bytes = file.read(kMaxKeyFileBytes + 1);
    auto* watcher = new QFutureWatcher<UnlockResult>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, &P2pIdentityStore::onUnlockFinished);
    watcher->setFuture(QtConcurrent::run([bytes, passphrase]() {
        UnlockResult result;
        result.seed = decryptFile(bytes, passphrase, &result.failure);
        return result;
    }));
}

void P2pIdentityStore::onUnlockFinished()
{
    auto* watcher = static_cast<QFutureWatcher<UnlockResult>*>(sender());
    const UnlockResult result = watcher->result();
    watcher->deleteLater();
    if (result.failure != Failure::None || !result.seed) {
        finish(result.failure == Failure::None ? Failure::CorruptFile : result.failure);
        return;
    }
    adoptSeed(*result.seed);
    // 保管した公開鍵と一致しなければ、別の鍵のファイル
    if (!isUnlocked() || m_key->publicKey() != readInfo(infoPath()).publicKey) {
        m_key.reset();
        finish(Failure::CorruptFile);
        return;
    }
    finish(Failure::None);
}

void P2pIdentityStore::lock()
{
    m_key.reset();
}

void P2pIdentityStore::remove()
{
    lock();
    if (readInfo(infoPath()).method == Method::Keychain) deleteFromKeychain();
    QFile::remove(keyFilePath());
    QFile::remove(infoPath());
}

// ---------------------------------------------------------------- 鍵の保管庫

#ifdef SBQ_HAVE_QTKEYCHAIN

void P2pIdentityStore::storeInKeychain(const std::shared_ptr<P2p::SecureBytes>& seed)
{
    m_pendingSeed = seed;
    auto* job = new QKeychain::WritePasswordJob(QString::fromLatin1(AppIdentity::kApplicationName), this);
    job->setAutoDelete(true);
    job->setInsecureFallback(false);
    job->setKey(QString::fromLatin1(kKeychainKey));
    QByteArray raw(reinterpret_cast<const char*>(seed->data()), kSeedSize);
    job->setBinaryData(raw);
    wipe(raw);
    connect(job, &QKeychain::Job::finished, this, &P2pIdentityStore::onKeychainWriteFinished);
    job->start();
}

void P2pIdentityStore::onKeychainWriteFinished(QKeychain::Job* job)
{
    const std::shared_ptr<P2p::SecureBytes> seed = std::move(m_pendingSeed);
    if (job->error() != QKeychain::NoError || !seed) {
        finish(Failure::KeychainUnavailable);
        return;
    }
    finishCreate(Method::Keychain, seed, QByteArray(), Failure::None);
}

void P2pIdentityStore::readFromKeychain()
{
    auto* job = new QKeychain::ReadPasswordJob(QString::fromLatin1(AppIdentity::kApplicationName), this);
    job->setAutoDelete(true);
    job->setInsecureFallback(false);
    job->setKey(QString::fromLatin1(kKeychainKey));
    connect(job, &QKeychain::Job::finished, this, &P2pIdentityStore::onKeychainReadFinished);
    job->start();
}

void P2pIdentityStore::onKeychainReadFinished(QKeychain::Job* job)
{
    auto* read = static_cast<QKeychain::ReadPasswordJob*>(job);
    QByteArray raw = read->binaryData();
    if (job->error() != QKeychain::NoError || raw.size() != kSeedSize) {
        wipe(raw);
        finish(job->error() == QKeychain::NoError ? Failure::CorruptFile : Failure::KeychainUnavailable);
        return;
    }
    P2p::SecureBytes seed(kSeedSize);
    if (!seed.isValid()) {
        wipe(raw);
        finish(Failure::Crypto);
        return;
    }
    std::copy(raw.cbegin(), raw.cend(), seed.data());
    wipe(raw);
    adoptSeed(seed);
    if (!isUnlocked() || m_key->publicKey() != readInfo(infoPath()).publicKey) {
        m_key.reset();
        finish(Failure::CorruptFile);
        return;
    }
    finish(Failure::None);
}

void P2pIdentityStore::deleteFromKeychain()
{
    auto* job = new QKeychain::DeletePasswordJob(QString::fromLatin1(AppIdentity::kApplicationName), this);
    job->setAutoDelete(true);
    job->setInsecureFallback(false);
    job->setKey(QString::fromLatin1(kKeychainKey));
    job->start();
}

#else

void P2pIdentityStore::storeInKeychain(const std::shared_ptr<P2p::SecureBytes>& /*seed*/)
{
    finish(Failure::KeychainUnavailable);
}

void P2pIdentityStore::readFromKeychain()
{
    finish(Failure::KeychainUnavailable);
}

void P2pIdentityStore::deleteFromKeychain()
{
}

#endif
