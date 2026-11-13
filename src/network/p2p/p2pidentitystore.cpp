/// @file p2pidentitystore.cpp
/// @brief プレイヤー鍵の保管の実装

#include "p2pidentitystore.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFutureWatcher>
#include <QStandardPaths>
#include <QtConcurrent/QtConcurrentRun>

#include <sodium.h>

#include "appidentity.h"
#include "p2pcbor.h"
#include "p2pownerfile.h"
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
/// 鍵の保管庫の項目のキー（サービス名はアプリ名 AppIdentity::kApplicationName）。標準の保存先の項目の名前で、ほかの保存先は
/// この後ろに保存先から決まる語を付ける（P2pIdentityStore::keychainItemKey）
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

/// ほかと共有している領域ごと、その場で消す（QtKeychain のジョブは渡した・読んだ種と同じ領域を持つ。data() で
/// 切り離して消すと、ジョブの持つ写しが残る。監査 2026-11-13 I-g）
void wipeShared(QByteArray& bytes)
{
    if (!bytes.isEmpty()) sodium_memzero(const_cast<char*>(bytes.constData()), static_cast<size_t>(bytes.size()));
    bytes.clear();
}

/// 古い鍵のファイルの中身を 0 で上書きしてから消す（作り直し・削除の後に、ディスクに古い種を残さない。SSD や
/// コピーオンライトのファイルシステムでは元の場所が上書きされる保証はないが、少なくともファイルとしては残さない）
void scrubAndRemove(const QString& path)
{
    QFile file(path);
    if (file.exists() && file.open(QIODevice::ReadWrite)) {
        const qint64 size = qMin<qint64>(file.size(), 64 * 1024);
        file.write(QByteArray(static_cast<qsizetype>(size), '\0'));
        file.flush();
        file.close();
    }
    QFile::remove(path);
}

/// 合言葉なしの鍵のファイルの形（種のところを 0 にしたもの）。決定的 CBOR なので、ファイルはこれの末尾の
/// kSeedSize バイトだけが違う。種を値の木（CborValue）に入れずに読み書きし、消せない写しを残さない
QByteArray plainKeyFileTemplate(int seedSize)
{
    using P2p::CborValue;
    return P2p::Cbor::encode(CborValue::array({CborValue::text("SBQ-IDENTITY"), CborValue::uint(1), CborValue::uint(0),
                                               CborValue::bytes(QByteArray(seedSize, '\0'))}));
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

QString P2pIdentityStore::keychainItemKey(const QString& directory, const QString& standardDataDirectory)
{
    const QString base = QString::fromLatin1(kKeychainKey);
    // 先に整えてから絶対パスにする（Windows の Qt は、整っていないパスにだけドライブを補うので、書き方で結果が変わる）
    const auto absolute = [](const QString& path) { return QDir::cleanPath(QDir(QDir::cleanPath(path)).absolutePath()); };
    const QString own = absolute(directory);
    if (!standardDataDirectory.isEmpty()) {
        // これまでの名前を使うのは、標準の場所の1人目の鍵だけ（P2pPlayerSlot::storageDirectory の p2p と、
        // P2pPlayerProfile::identityDirectory の identity。2人目からの p2p-2/identity などは別の項目にする）
        const QString standard = absolute(QDir(standardDataDirectory).filePath(QStringLiteral("p2p/identity")));
        if (own == standard) return base;
    }
    const QByteArray digest = QCryptographicHash::hash(own.toUtf8(), QCryptographicHash::Sha256);
    return base + QLatin1Char('@') + QString::fromLatin1(digest.left(8).toHex());
}

QString P2pIdentityStore::keychainItemKey() const
{
    return keychainItemKey(m_directory, QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
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

bool P2pIdentityStore::writeInfo(Method method, const QByteArray& publicKey)
{
    using P2p::CborValue;
    const QByteArray bytes = P2p::Cbor::encode(CborValue::array(
        {CborValue::text("SBQ-IDENTITY-INFO"), CborValue::uint(1), CborValue::uint(static_cast<quint64>(method)),
         CborValue::bytes(publicKey)}));
    return P2pOwnerFile::writeAtomically(infoPath(), bytes);
}

// ---------------------------------------------------------------- 鍵のファイル

QByteArray P2pIdentityStore::encryptFile(const P2p::SecureBytes& seed, const QString& passphrase, quint64 opsLimit,
                                         quint64 memLimitBytes)
{
    using P2p::CborValue;
    if (!P2p::Crypto::initialize() || !seed.isValid() || seed.size() != kSeedSize) return {};
    if (passphrase.isEmpty()) {
        QByteArray out = plainKeyFileTemplate(kSeedSize);
        std::copy(seed.data(), seed.data() + kSeedSize, out.end() - kSeedSize);
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
    // 合言葉なし: 決まった形の末尾が種（値の木に入れない）
    const QByteArray plain = plainKeyFileTemplate(kSeedSize);
    if (fileBytes.size() == plain.size()
        && QByteArrayView(fileBytes).first(plain.size() - kSeedSize) == QByteArrayView(plain).first(plain.size() - kSeedSize)) {
        auto seed = std::make_shared<P2p::SecureBytes>(kSeedSize);
        if (!seed->isValid()) return fail(Failure::Crypto);
        std::copy(fileBytes.cend() - kSeedSize, fileBytes.cend(), seed->data());
        if (failure) *failure = Failure::None;
        return seed;
    }
    const std::optional<P2p::CborValue> value = P2p::Cbor::decode(fileBytes);
    if (!value || !value->isArray() || value->items().size() < 3) return fail(Failure::CorruptFile);
    const quint64 scheme = value->items()[2].isUInt() ? value->items()[2].toUInt() : 99;
    auto seed = std::make_shared<P2p::SecureBytes>(kSeedSize);
    if (!seed->isValid()) return fail(Failure::Crypto);

    // 合言葉なしの形は上で読んだ（ここに来るのは形の合わないファイル）
    if (scheme == 0) return fail(Failure::CorruptFile);

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
        QByteArray fileBytes = encryptFile(*seed, QString(), 0, 0);
        finishCreate(method, seed, fileBytes, Failure::None);
        wipe(fileBytes);   // 合言葉なしのファイルの中身は種そのもの
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
    // 前の鍵のファイルは、別名に退けてから新しいものを書き、書けたら中身を消して削除する（置き換えは名前を付け替える
    // だけで、古い中身を上書きしない）。書けなければ元に戻す
    const QString previous = keyFilePath() + QStringLiteral(".previous");
    scrubAndRemove(previous);
    const bool hadPrevious = QFile::exists(keyFilePath()) && QFile::rename(keyFilePath(), previous);
    if (method != Method::Keychain && !P2pOwnerFile::writeAtomically(keyFilePath(), fileBytes)) {
        if (hadPrevious) QFile::rename(previous, keyFilePath());
        finish(Failure::Storage);
        return;
    }
    if (hadPrevious) scrubAndRemove(previous);
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
    QByteArray bytes = file.read(kMaxKeyFileBytes + 1);
    file.close();
    auto* watcher = new QFutureWatcher<UnlockResult>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, &P2pIdentityStore::onUnlockFinished);
    // 読んだファイル（合言葉なしなら種そのもの）はワーカーへ移し、読み終えたら消す（ほかに持ち主がいないので消せる）
    watcher->setFuture(QtConcurrent::run([bytes = std::move(bytes), passphrase]() mutable {
        UnlockResult result;
        result.seed = decryptFile(bytes, passphrase, &result.failure);
        wipe(bytes);
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
    scrubAndRemove(keyFilePath());
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
    job->setKey(keychainItemKey());
    // ジョブは同じ領域を共有して持つ。書き終えたら（onKeychainWriteFinished）その場で消す
    m_pendingRaw = QByteArray(reinterpret_cast<const char*>(seed->data()), kSeedSize);
    job->setBinaryData(m_pendingRaw);
    connect(job, &QKeychain::Job::finished, this, &P2pIdentityStore::onKeychainWriteFinished);
    job->start();
}

void P2pIdentityStore::onKeychainWriteFinished(QKeychain::Job* job)
{
    wipeShared(m_pendingRaw);
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
    job->setKey(keychainItemKey());
    connect(job, &QKeychain::Job::finished, this, &P2pIdentityStore::onKeychainReadFinished);
    job->start();
}

void P2pIdentityStore::onKeychainReadFinished(QKeychain::Job* job)
{
    auto* read = static_cast<QKeychain::ReadPasswordJob*>(job);
    // ジョブの持つ種と同じ領域を共有する。その場で消す（wipeShared）と、ジョブの写しも消える
    QByteArray raw = read->binaryData();
    if (job->error() != QKeychain::NoError || raw.size() != kSeedSize) {
        wipeShared(raw);
        finish(job->error() == QKeychain::NoError ? Failure::CorruptFile : Failure::KeychainUnavailable);
        return;
    }
    P2p::SecureBytes seed(kSeedSize);
    if (!seed.isValid()) {
        wipeShared(raw);
        finish(Failure::Crypto);
        return;
    }
    std::copy(raw.cbegin(), raw.cend(), seed.data());
    wipeShared(raw);
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
    job->setKey(keychainItemKey());
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
