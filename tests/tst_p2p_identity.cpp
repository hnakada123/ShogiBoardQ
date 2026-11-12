/// @file tst_p2p_identity.cpp
/// @brief プレイヤー鍵の保管の試験（暗号化したファイル・誤ったパスフレーズ・書き換えたパラメータ・範囲外・権限。実装仕様 13.1節）

#include <QtTest>

#include <QSignalSpy>
#include <QTemporaryDir>

#include "p2pcbor.h"
#include "p2pidentitystore.h"
#include "p2pplayerprofile.h"
#include "p2pplayerslot.h"

using namespace P2p;

namespace {

SecureBytes seedOf(char fill)
{
    SecureBytes seed(32);
    std::fill(seed.data(), seed.data() + 32, static_cast<unsigned char>(fill));
    return seed;
}

bool sameSeed(const SecureBytes& a, const SecureBytes& b)
{
    return a.size() == b.size() && std::equal(a.data(), a.data() + a.size(), b.data());
}

/// 暗号化した鍵のファイルの n 番目の値を書き換える
QByteArray withField(const QByteArray& file, std::size_t index, const CborValue& value)
{
    std::optional<CborValue> root = Cbor::decode(file);
    std::vector<CborValue> items = root->items();
    items[index] = value;
    return Cbor::encode(CborValue::array(std::move(items)));
}

} // namespace

class TestP2pIdentity : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    /// 試験を速くするため、受け付ける範囲の下限のパラメータで暗号化する
    static constexpr quint64 kOps = P2pIdentityStore::kMinOpsLimit;
    static constexpr quint64 kMem = P2pIdentityStore::kMinMemLimitBytes;

private slots:
    void initTestCase()
    {
        QVERIFY(Crypto::initialize());
        QVERIFY(m_dir.isValid());
    }

    /// 鍵の保管庫の項目は保存先ごとに別にする。同じ PC の2人目からのプレイヤー・環境変数で分けた保存先・試験が、
    /// 1人目の鍵を上書き・削除しない
    void keychainItemIsSeparatePerStorageFolder()
    {
        const QString standard = QStringLiteral("/home/u/.local/share/ShogiBoardQ");
        const QString base = QStringLiteral("player-key-v1");
        const auto identityOf = [](const QString& dataDirectory, int player) {
            return P2pPlayerProfile::identityDirectory(P2pPlayerSlot::storageDirectory(dataDirectory, player));
        };
        // 標準の場所の1人目の鍵は、これまでと同じ名前（実際の保存先の組み立てと合っている）
        QCOMPARE(identityOf(standard, 1), standard + QStringLiteral("/p2p/identity"));
        QCOMPARE(P2pIdentityStore::keychainItemKey(identityOf(standard, 1), standard), base);
        QCOMPARE(P2pIdentityStore::keychainItemKey(standard + QStringLiteral("/p2p/../p2p/identity/"),
                                                   standard + QLatin1Char('/')),
                 base);

        // 同じ PC の2人目・3人目は、1人目とも互いとも別の項目
        const QString player2 = P2pIdentityStore::keychainItemKey(identityOf(standard, 2), standard);
        const QString player3 = P2pIdentityStore::keychainItemKey(identityOf(standard, 3), standard);
        QCOMPARE(player2, QStringLiteral("player-key-v1@346eb5b0826c6407"));
        QVERIFY(player3.startsWith(base + QLatin1Char('@')));
        QVERIFY(player3 != player2);

        // 環境変数で分けた保存先は、保存先から決まる別の名前。同じ保存先は、書き方が違っても同じ名前
        const QString other = QStringLiteral("/home/u/sbq-second/data/ShogiBoardQ");
        const QString second = P2pIdentityStore::keychainItemKey(identityOf(other, 1), standard);
        QCOMPARE(second, QStringLiteral("player-key-v1@fd23df281a69749c"));
        QCOMPARE(P2pIdentityStore::keychainItemKey(
                     QStringLiteral("/home/u/sbq-second/data/./ShogiBoardQ/p2p/identity/"), standard),
                 second);
        QVERIFY(P2pIdentityStore::keychainItemKey(identityOf(other, 2), standard) != second);

        // 標準の場所のほかのフォルダと、標準の場所が分からないときは、これまでの名前を使わない
        QVERIFY(P2pIdentityStore::keychainItemKey(standard, standard) != base);
        QVERIFY(P2pIdentityStore::keychainItemKey(standard + QStringLiteral("/p2p"), standard) != base);
        QVERIFY(P2pIdentityStore::keychainItemKey(identityOf(standard, 1), QString()) != base);
    }

    void passphraseFileRoundTrip()
    {
        const SecureBytes seed = seedOf('\x07');
        const QByteArray file = P2pIdentityStore::encryptFile(seed, QStringLiteral("正しいパスフレーズ"), kOps, kMem);
        QVERIFY(!file.isEmpty());
        // 種がそのまま入っていない
        QVERIFY(!file.contains(QByteArray(32, '\x07')));
        P2pIdentityStore::Failure failure = P2pIdentityStore::Failure::Busy;
        const auto decrypted = P2pIdentityStore::decryptFile(file, QStringLiteral("正しいパスフレーズ"), &failure);
        QCOMPARE(failure, P2pIdentityStore::Failure::None);
        QVERIFY(decrypted && sameSeed(*decrypted, seed));
        // 保存のたびに salt と nonce を作り直す
        QVERIFY(P2pIdentityStore::encryptFile(seed, QStringLiteral("正しいパスフレーズ"), kOps, kMem) != file);
    }

    void wrongPassphraseAndTamperingAreRejected()
    {
        const QByteArray file = P2pIdentityStore::encryptFile(seedOf('\x01'), QStringLiteral("pass"), kOps, kMem);
        P2pIdentityStore::Failure failure = P2pIdentityStore::Failure::None;
        QVERIFY(!P2pIdentityStore::decryptFile(file, QStringLiteral("wrong"), &failure));
        QCOMPARE(failure, P2pIdentityStore::Failure::WrongPassphrase);
        // パラメータ（追加の認証データ）を書き換えると復号に失敗する
        const QByteArray tampered = withField(file, 3, CborValue::uint(kOps + 1));
        QVERIFY(!P2pIdentityStore::decryptFile(tampered, QStringLiteral("pass"), &failure));
        QCOMPARE(failure, P2pIdentityStore::Failure::WrongPassphrase);
    }

    void outOfRangeParametersAreRejectedBeforeDerivation()
    {
        const QByteArray file = P2pIdentityStore::encryptFile(seedOf('\x02'), QStringLiteral("pass"), kOps, kMem);
        P2pIdentityStore::Failure failure = P2pIdentityStore::Failure::None;
        QElapsedTimer timer;
        timer.start();
        QVERIFY(!P2pIdentityStore::decryptFile(withField(file, 3, CborValue::uint(11)), QStringLiteral("pass"), &failure));
        QCOMPARE(failure, P2pIdentityStore::Failure::CorruptFile);
        QVERIFY(!P2pIdentityStore::decryptFile(withField(file, 4, CborValue::uint(2ULL << 30)), QStringLiteral("pass"),
                                               &failure));
        QCOMPARE(failure, P2pIdentityStore::Failure::CorruptFile);
        QVERIFY(!P2pIdentityStore::decryptFile(withField(file, 4, CborValue::uint(1ULL << 20)), QStringLiteral("pass"),
                                               &failure));
        QCOMPARE(failure, P2pIdentityStore::Failure::CorruptFile);
        // 鍵の導出を始めていない（範囲外は読む前に断る）
        QVERIFY(timer.elapsed() < 1000);
        // 壊れたファイル・大きすぎるファイル
        QVERIFY(!P2pIdentityStore::decryptFile(QByteArray("not cbor"), QStringLiteral("pass"), &failure));
        QCOMPARE(failure, P2pIdentityStore::Failure::CorruptFile);
        QVERIFY(!P2pIdentityStore::decryptFile(QByteArray(2048, '\x80'), QStringLiteral("pass"), &failure));
        QCOMPARE(failure, P2pIdentityStore::Failure::CorruptFile);
    }

    void plainFileIsCreatedWithOwnerOnlyPermissions()
    {
        const QString dir = m_dir.filePath(QStringLiteral("plain"));
        P2pIdentityStore store(dir);
        QCOMPARE(store.method(), P2pIdentityStore::Method::None);
        QSignalSpy finished(&store, &P2pIdentityStore::finished);
        store.create(P2pIdentityStore::Method::Plain);
        QCOMPARE(finished.count(), 1);
        QCOMPARE(finished.first().at(0).value<P2pIdentityStore::Failure>(), P2pIdentityStore::Failure::None);
        QVERIFY(store.isUnlocked());
        const QByteArray publicKey = store.publicKey();
        QCOMPARE(publicKey.size(), 32);
        QCOMPARE(store.method(), P2pIdentityStore::Method::Plain);
#ifdef Q_OS_UNIX
        const QFileDevice::Permissions perms = QFile::permissions(QDir(dir).filePath(QStringLiteral("player.key")));
        QCOMPARE(perms & (QFileDevice::ReadGroup | QFileDevice::ReadOther | QFileDevice::WriteGroup | QFileDevice::WriteOther),
                 QFileDevice::Permissions());
#endif
        // 読み込み直すと同じ公開鍵
        store.lock();
        QVERIFY(!store.isUnlocked());
        store.unlock();
        QTRY_VERIFY(store.isUnlocked());
        QCOMPARE(store.publicKey(), publicKey);
        // 作り直すとプレイヤーID が変わり、古い鍵は消える
        store.remove();
        QCOMPARE(store.method(), P2pIdentityStore::Method::None);
        store.create(P2pIdentityStore::Method::Plain);
        QVERIFY(store.publicKey() != publicKey);
    }

    void passphraseStoreUnlocksOffTheGuiThread()
    {
        P2pIdentityStore store(m_dir.filePath(QStringLiteral("pass")));
        QSignalSpy finished(&store, &P2pIdentityStore::finished);
        store.create(P2pIdentityStore::Method::Passphrase, QStringLiteral("秘密の言葉"));
        // 鍵の導出は別のスレッド（ここでは終わっていない）
        QVERIFY(store.isBusy());
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 30000);
        QCOMPARE(finished.last().at(0).value<P2pIdentityStore::Failure>(), P2pIdentityStore::Failure::None);
        const QByteArray publicKey = store.publicKey();
        store.lock();
        store.unlock(QStringLiteral("違う言葉"));
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 2, 30000);
        QCOMPARE(finished.last().at(0).value<P2pIdentityStore::Failure>(), P2pIdentityStore::Failure::WrongPassphrase);
        QVERIFY(!store.isUnlocked());
        store.unlock(QStringLiteral("秘密の言葉"));
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 3, 30000);
        QVERIFY(store.isUnlocked());
        QCOMPARE(store.publicKey(), publicKey);
    }
};

QTEST_GUILESS_MAIN(TestP2pIdentity)
#include "tst_p2p_identity.moc"
