#ifndef P2PPLAYERPROFILE_H
#define P2PPLAYERPROFILE_H

/// @file p2pplayerprofile.h
/// @brief この PC の1人のプレイヤーの保存先（ロック・対局の記録・既知の相手・プレイヤー鍵。実装仕様 9章）

#include <QObject>
#include <QString>

#include <memory>

class P2pIdentityStore;
class P2pKnownPeers;
class P2pRecordStore;
class QLockFile;

/**
 * @brief 1人のプレイヤーの保存先（`AppPaths::dataDirectory()/p2p/`、2人目からは `p2p-2/`…。P2pPlayerSlot）
 *
 * 同じ保存先を同時に使えるアプリは1つ（ロック）。開いている間だけ、対局の記録・既知の相手・プレイヤー鍵を使う。
 * 所有者だけが読み書きできる権限で作る。通信プロセスにこの場所を渡さない。
 */
class P2pPlayerProfile : public QObject
{
    Q_OBJECT
public:
    /// root は保存先のフォルダ、playerNumber はこの PC の何人目のプレイヤーか（1〜）
    explicit P2pPlayerProfile(const QString& root, int playerNumber = 1, QObject* parent = nullptr);
    ~P2pPlayerProfile() override;

    /// この PC で空いている最初の番号のプレイヤーの保存先を開いて返す。1人目から順に試し、ほかのアプリが使っている
    /// 番号は飛ばす。どれも使えなければ nullptr
    static P2pPlayerProfile* acquire(const QString& dataDirectory, QObject* parent = nullptr);
    /// 保存先 root のプレイヤー鍵の場所（OS の鍵の保管庫の項目も、この場所ごとに分ける）
    static QString identityDirectory(const QString& root);

    QString root() const { return m_root; }
    int playerNumber() const { return m_playerNumber; }

    /// 保存先を使う権利を取り、前回のアプリが残したジャーナルを結果未確定の証跡に直し、既知の相手を読む。
    /// ほかのアプリが使っていれば false
    bool open();
    /// プレイヤー鍵をメモリから消し、保存先を手放す
    void close();
    bool isOpen() const;

    P2pRecordStore* records() const { return m_records.get(); }
    P2pKnownPeers* knownPeers() const { return m_knownPeers.get(); }
    P2pIdentityStore* identity() const { return m_identity; }

private:
    QString m_root;
    int m_playerNumber = 1;
    std::unique_ptr<QLockFile> m_lock;
    std::unique_ptr<P2pRecordStore> m_records;
    std::unique_ptr<P2pKnownPeers> m_knownPeers;
    P2pIdentityStore* m_identity = nullptr;   ///< 子オブジェクト
};

#endif // P2PPLAYERPROFILE_H
