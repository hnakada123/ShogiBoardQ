#ifndef P2PIDENTITYSTORE_H
#define P2PIDENTITYSTORE_H

/// @file p2pidentitystore.h
/// @brief プレイヤー鍵の生成・保管・読み込み・作り直し（OS の鍵の保管庫か、暗号化したファイル。実装仕様 5.2節）

#include <QByteArray>
#include <QObject>
#include <QString>

#include <memory>

#include "p2psignature.h"

#ifdef SBQ_HAVE_QTKEYCHAIN
namespace QKeychain { class Job; }
#endif

/**
 * @brief プレイヤー鍵の保管
 *
 * 保管の方法は次の3つ（実装仕様 5.2節）。どれを使っているかと公開鍵は、`identity/info` に書く
 * （秘密は書かない）。鍵は「P2P対局」を使う間だけ読み込み、使い終えたら lock() で消す。
 *
 * - Keychain: OS の鍵の保管庫（QtKeychain。サービス名はアプリ名 AppIdentity::kApplicationName で、保存先ごとに
 *   別の項目 keychainItemKey）。
 *   使えなければ失敗を返し、暗号化しない保存へ黙って切り替えない（setInsecureFallback は使わない）。
 * - Passphrase: パスフレーズから Argon2id で導いた鍵と XChaCha20-Poly1305 で暗号化したファイル。
 *   鍵の導出は GUI スレッドの外で行う。
 * - Plain: 利用者が明示的に選んだときだけの、暗号化しないファイル。
 *
 * ファイルは所有者だけが読み書きできる権限で、原子的に書く。
 */
class P2pIdentityStore : public QObject
{
    Q_OBJECT
public:
    enum class Method { None = 0, Keychain = 1, Passphrase = 2, Plain = 3 };
    Q_ENUM(Method)

    /// 失敗の種類（画面の文は呼ぶ側が選ぶ）
    enum class Failure {
        None,
        KeychainUnavailable,  ///< 鍵の保管庫が使えない（このビルドに無い・保管庫がエラーを返した）
        WrongPassphrase,      ///< パスフレーズが違う、またはファイルが改変された
        CorruptFile,          ///< 鍵のファイルが読めない・範囲外のパラメータ
        Storage,              ///< ファイルの読み書きの失敗
        Crypto,               ///< libsodium を使えない
        Busy,                 ///< ほかの操作の途中
    };
    Q_ENUM(Failure)

    /// 新しく作るファイルの Argon2id のパラメータ（libsodium の MODERATE）
    static constexpr quint64 kOpsLimit = 3;
    static constexpr quint64 kMemLimitBytes = 256ULL * 1024 * 1024;
    /// 読むときに受け付ける範囲（細工したファイルで大量のメモリと時間を使わせない）
    static constexpr quint64 kMinOpsLimit = 1;
    static constexpr quint64 kMaxOpsLimit = 10;
    static constexpr quint64 kMinMemLimitBytes = 64ULL * 1024 * 1024;
    static constexpr quint64 kMaxMemLimitBytes = 1024ULL * 1024 * 1024;

    /// directory は `AppPaths::dataDirectory()/p2p/identity`（試験では一時フォルダ）
    explicit P2pIdentityStore(const QString& directory, QObject* parent = nullptr);
    ~P2pIdentityStore() override;

    /// このビルドで OS の鍵の保管庫を使えるか（QtKeychain を組み込んだか）
    static bool keychainSupported();
    /// 鍵の保管庫の項目のキー。保存先（directory）ごとに別の項目にする。同じ PC の2人目からのプレイヤー
    /// （P2pPlayerSlot）・環境変数で分けた保存先（SHOGIBOARDQ_DATA_HOME）・試験が、1人目の鍵を上書き・削除しない
    /// ためである。標準のデータの場所（standardDataDirectory）の1人目の鍵の場所（p2p/identity）なら
    /// "player-key-v1"、それ以外は、その後ろに "@" と、保存先の絶対パスの SHA-256 の先頭16桁（16進）を付ける
    static QString keychainItemKey(const QString& directory, const QString& standardDataDirectory);

    /// 保管している方法（まだ作っていなければ None）
    Method method() const;
    /// 保管している鍵の公開鍵（プレイヤーID。作っていなければ空）
    QByteArray publicKey() const;
    /// 鍵を読み込み済みか
    bool isUnlocked() const { return m_key && m_key->isValid(); }
    /// 読み込んだ鍵（isUnlocked のときだけ）
    const P2p::SigningKey* key() const { return isUnlocked() ? m_key.get() : nullptr; }
    /// 操作の途中か
    bool isBusy() const { return m_busy; }

    /// 新しい鍵を作って method で保管する（前の鍵があれば消す）。結果は finished
    void create(Method method, const QString& passphrase = QString());
    /// 保管した鍵を読み込む（Passphrase のときだけ passphrase を使う）。結果は finished
    void unlock(const QString& passphrase = QString());
    /// 読み込んだ鍵をメモリから消す
    void lock();
    /// 鍵を消す（保管庫の項目とファイルも）。プレイヤーID は変わる
    void remove();

    /// 鍵のファイルの中身を読んで鍵を取り出す（試験と unlock の本体。GUI スレッドの外で呼べる）
    static std::shared_ptr<P2p::SecureBytes> decryptFile(const QByteArray& fileBytes, const QString& passphrase,
                                                         Failure* failure);
    /// 種から鍵のファイルを作る（passphrase が空なら暗号化しない形。GUI スレッドの外で呼べる）
    static QByteArray encryptFile(const P2p::SecureBytes& seed, const QString& passphrase, quint64 opsLimit,
                                  quint64 memLimitBytes);

signals:
    /// create・unlock が終わった（failure が None なら成功）
    void finished(P2pIdentityStore::Failure failure);

private slots:
    void onCreateFinished();
    void onUnlockFinished();
#ifdef SBQ_HAVE_QTKEYCHAIN
    void onKeychainWriteFinished(QKeychain::Job* job);
    void onKeychainReadFinished(QKeychain::Job* job);
#endif

private:
    void finish(Failure failure);
    void finishCreate(Method method, const std::shared_ptr<P2p::SecureBytes>& seed, const QByteArray& fileBytes,
                      Failure failure);
    void adoptSeed(const P2p::SecureBytes& seed);
    bool writeInfo(Method method, const QByteArray& publicKey);
    QString infoPath() const;
    QString keyFilePath() const;

    /// この保存先の鍵の保管庫の項目のキー
    QString keychainItemKey() const;
    void storeInKeychain(const std::shared_ptr<P2p::SecureBytes>& seed);
    void readFromKeychain();
    void deleteFromKeychain();

    QString m_directory;
    std::unique_ptr<P2p::SigningKey> m_key;
    std::shared_ptr<P2p::SecureBytes> m_pendingSeed;   ///< 保管庫へ書いている間の種
    QByteArray m_pendingRaw;   ///< 鍵の保管庫のジョブに渡した種（ジョブと領域を共有する。書き終えたらその場で消す）
    bool m_busy = false;
};

#endif // P2PIDENTITYSTORE_H
