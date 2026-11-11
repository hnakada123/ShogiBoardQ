#ifndef P2PRECORDSTORE_H
#define P2PRECORDSTORE_H

/// @file p2precordstore.h
/// @brief P2P対局の保存先（ロック・ジャーナル・証跡・履歴の情報・既知の相手。実装仕様 9章）

#include <QByteArray>
#include <QFile>
#include <QList>
#include <QLockFile>
#include <QString>

#include <memory>
#include <optional>
#include <vector>

#include "p2pevidence.h"

/**
 * @brief 1局の対局ジャーナル（追記型。1件ごとに長さとチェックサムを付け、ストレージへの同期まで済ませる）
 *
 * 送る前の提案と ACK の前の受理証明は、ここへ書き終えてから外へ送る（実装仕様 9章）。書けなければ false を返し、
 * 呼ぶ側は署名・着手を進めずに中断する。
 */
class P2pJournal
{
public:
    /// 1局のジャーナルの上限（実装仕様 4章）
    static constexpr qint64 kMaxBytes = 16LL * 1024 * 1024;

    bool open(const QString& path);
    bool isOpen() const { return m_file.isOpen(); }
    /// 1件を追記する（上限を超える・書けない・同期できないときは false）
    bool append(const P2p::CborValue& record);
    void close() { m_file.close(); }
    QString path() const { return m_file.fileName(); }

    /// 完全な件だけを読む（途中で切れた末尾・チェックサムの合わない件から後は読まない）
    static std::vector<P2p::CborValue> readAll(const QString& path);

private:
    QFile m_file;
};

/// 履歴に出す、証跡の外の情報（表示名は証跡に入れないのでここに置く）
struct P2pHistoryInfo {
    quint8 mySide = 1;             ///< 1=先手、2=後手
    QString opponentDisplayName;   ///< 相手が名乗った表示名
    QString localEnding;           ///< ローカルの終局理由（"resign" など。protocol の語）
    quint64 endedAt = 0;           ///< 終わった時刻（UNIX秒。手元の時計）
    QString kifuPath;              ///< 保存した通常棋譜（あれば）
};

/**
 * @brief `AppPaths::dataDirectory()/p2p/` の保存物
 *
 * 所有者だけが読み書きできる権限で作る。同時に使えるアプリは1つ（QLockFile）。通信プロセスにこの場所を渡さない。
 */
class P2pRecordStore
{
public:
    /// `p2p/` 全体の大きさの上限（実装仕様 9章。超えたら新しい対局の前に利用者に尋ねる）
    static constexpr qint64 kMaxTotalBytes = 1024LL * 1024 * 1024;

    struct Entry {
        QByteArray gameId;
        QString evidencePath;
        P2pHistoryInfo info;
        qint64 bytes = 0;
        bool hasJournal = false;   ///< 相手の矛盾した署名の証明を残したジャーナルがある
    };

    explicit P2pRecordStore(const QString& root);
    ~P2pRecordStore();

    QString root() const { return m_root; }
    QString identityDirectory() const;

    /// p2p/ を使う権利を取る（取れなければ false。ほかのアプリが使っている）
    bool lock();
    void unlock();
    bool isLocked() const { return m_lock && m_lock->isLocked(); }

    /// 新しい対局のジャーナル（gameId の16進のファイル名）
    std::unique_ptr<P2pJournal> createJournal(const QByteArray& gameId);
    /**
     * @brief 証跡を原子的に保存し、ジャーナルを消す（keepJournal なら残す）
     * @return 保存した証跡の場所（失敗なら空）
     */
    QString saveEvidence(const P2p::Evidence& evidence, const P2pHistoryInfo& info, bool keepJournal);
    /// 前回のアプリが残したジャーナルを、結果未確定の証跡に直す（再起動後は対局を続けない）
    int recoverJournals();
    /// 保存した対局の一覧（新しい順）
    QList<Entry> history() const;
    /// 対局の証跡・情報・ジャーナルを消す
    bool removeRecord(const QByteArray& gameId);
    /// p2p/ 全体の大きさ
    qint64 totalBytes() const;

    /// ジャーナルの件から証跡を組み立てる（読めない件は飛ばす）
    static std::optional<P2p::Evidence> evidenceFromJournal(const std::vector<P2p::CborValue>& records,
                                                            P2pHistoryInfo* info, bool* hasConflict);

    /// 既知ピアのキャッシュ（通信プロセスに起動時に渡す multiaddr。64件まで）
    QStringList knownPeerAddresses() const;
    void saveKnownPeerAddresses(const QStringList& addresses);

private:
    QString journalDirectory() const;
    QString evidenceDirectory() const;
    bool ensureDirectory(const QString& path) const;

    QString m_root;
    std::unique_ptr<QLockFile> m_lock;
};

#endif // P2PRECORDSTORE_H
