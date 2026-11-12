#ifndef P2PRECORDSTORE_H
#define P2PRECORDSTORE_H

/// @file p2precordstore.h
/// @brief P2P対局の記録（ジャーナル・証跡・履歴の情報。実装仕様 9章）

#include <QByteArray>
#include <QFile>
#include <QList>
#include <QString>

#include <memory>
#include <optional>
#include <vector>

#include "p2pevidence.h"
#include "p2psettlement.h"

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
    P2p::Settlement settlement = P2p::Settlement::None;   ///< 結果の確定のしかた（ファイルには settlementName の語で書く）
    quint64 endedAt = 0;           ///< 終わった時刻（UNIX秒。手元の時計）
    QString kifuPath;              ///< 保存した通常棋譜（あれば）
};

/**
 * @brief プレイヤーの保存先（P2pPlayerProfile。`p2p/` など）の中の対局の記録
 *
 * 所有者だけが読み書きできる権限で作る。保存先を使う権利（ロック）は P2pPlayerProfile が取る。通信プロセスにこの場所を
 * 渡さない。
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

    QString root() const { return m_root; }

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

private:
    QString journalDirectory() const;
    QString evidenceDirectory() const;
    bool ensureDirectory(const QString& path) const;

    QString m_root;
};

#endif // P2PRECORDSTORE_H
