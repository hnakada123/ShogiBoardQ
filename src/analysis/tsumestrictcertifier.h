#ifndef TSUMESTRICTCERTIFIER_H
#define TSUMESTRICTCERTIFIER_H

/// @file tsumestrictcertifier.h
/// @brief 長手数詰将棋の厳密証明ジョブ（全主手順余詰なし・駒余りなし・全1枚除去の確定判定・チェックポイント対応）

#include "matequeryloop.h"
#include "tsumeshogiverifier.h"
#include "tsumeverificationjob.h"

#include <QJsonObject>
#include <QString>
#include <QStringList>

#include <map>
#include <memory>
#include <optional>
#include <string>

/**
 * @brief 候補局面1つを厳密基準で証明・不要駒トリミングする引き出し型ジョブ
 *
 * 1. CheckBase: 目標手数ちょうどで詰むかを確認（自然終了 MinLength 前提）
 * 2. VerifyBase: TsumeshogiVerifier で全主手順の一意性＋駒余りなしを証明
 * 3. TrimProbe / VerifyTrimmed: 玉以外の全盤上駒・攻方持駒を1枚ずつ玉方持駒へ移し、
 *    目標手数・一意性・駒余りなしが保たれる不要駒があれば除去して最初から再走査する。
 *    判定不能（Unknown）は1つでもあれば「必要駒の証明にならない」ため全体を棄却する。
 * 4. 全1枚除去の確定判定完了後に Certified を返す。
 *
 * 途中状態を toJson() / fromJson() で保存・復元でき、検証キャッシュも含めて中断再開できる。
 */
class TsumeStrictCertifier : public MateQuerySource
{
public:
    struct Settings {
        int targetMoves = 101;
        qint64 totalBudgetMs = 1800000;        ///< 1候補の厳密証明・トリミング全体の時間予算(ms)
        int queryMaxMs = 15000;                ///< 1回のエンジン問い合わせ上限(ms)
        bool allowFinalMoveAlternatives = false; ///< 既定 false: 最終手も完全一意を要求
        bool requireNoSurplus = true;          ///< 既定 true: 駒余りを完全禁止
        bool strictMinLength = true;           ///< 既定 true: 自然終了した MinLength 応答を前提とする
        int maxVisitedNodes = 200000;
        int maxCacheEntries = 200000;
    };

    enum class Stage {
        CheckBase,
        VerifyBase,
        TrimProbe,
        VerifyTrimmed,
        Certified,
        Rejected,
    };

    enum class RejectionReason {
        None,
        Invalid,
        WrongLength,
        NoMate,
        Multiple,
        Surplus,
        Inconclusive,
    };

    struct Outcome {
        QString sfen;
        QStringList pv;
        int removedPieces = 0;
        int queries = 0;
        qint64 spentMs = 0;
    };

    struct CookInfo {
        QString mainMove;
        QString cookMove;
        int ply = -1;
    };

    TsumeStrictCertifier(const QString& sfen, const Settings& settings);
    ~TsumeStrictCertifier() override;

    /// チェックポイント JSON から途中状態を復元する（不正なら nullptr）
    static std::unique_ptr<TsumeStrictCertifier> fromJson(const QJsonObject& json, const Settings& settings);
    /// 現在の途中状態と確定済み問い合わせキャッシュを JSON 化する
    QJsonObject toJson() const;

    /// 外部の確定キャッシュ（厳密応答のみ）を初期注入する
    void seedVerifiedCache(const std::map<std::string, TsumeshogiVerifier::CacheEntry>& entries);
    /// このジョブ中に確定した問い合わせキャッシュを取り出す
    const std::map<std::string, TsumeshogiVerifier::CacheEntry>& verifiedCache() const { return m_verifiedCache; }

    std::optional<MateQuery> nextQuery() override;
    void submit(const MateAnswer& answer, qint64 elapsedMs) override;
    bool isDone() const override { return m_stage == Stage::Certified || m_stage == Stage::Rejected; }

    void abort();

    Stage stage() const { return m_stage; }
    RejectionReason rejectionReason() const { return m_rejectionReason; }
    const std::optional<Outcome>& outcome() const { return m_outcome; }
    const std::optional<CookInfo>& cookInfo() const { return m_cookInfo; }
    QString candidateSfen() const { return m_candidateSfen; }
    QString baseSfen() const { return m_baseSfen; }
    int queries() const { return m_queries; }
    qint64 spentMs() const { return m_spentMs; }
    int trimIndex() const { return m_trimIndex; }
    int trimTotal() const { return static_cast<int>(m_trimPositions.size()); }
    int removedPieces() const { return m_removedPieces; }
    static QString stageName(Stage stage);
    static QString rejectionReasonName(RejectionReason reason);

private:
    TsumeStrictCertifier() = default;

    TsumeshogiVerifier::Options verifierOptions() const;
    int queryLimitMs() const;
    void startVerification(const QString& sfen, Stage stage);
    void syncVerifierCache();
    void finishVerification();
    void startTrimming(const QString& sfen, const QStringList& pv);
    std::optional<MateQuery> nextTrimQuery();
    void onTrimProbeAnswer(const MateAnswer& answer);
    void reject(RejectionReason reason);

    Settings m_settings;
    Stage m_stage = Stage::CheckBase;
    RejectionReason m_rejectionReason = RejectionReason::None;
    QString m_candidateSfen;
    QString m_baseSfen;
    QStringList m_basePv;
    QStringList m_trimPositions;
    int m_trimIndex = 0;
    QString m_trimSfen;
    bool m_trimProvenInTarget = false;
    int m_removedPieces = 0;
    int m_queries = 0;
    qint64 m_spentMs = 0;

    std::unique_ptr<TsumeVerificationJob> m_verification;
    std::map<std::string, TsumeshogiVerifier::CacheEntry> m_verifiedCache;
    std::optional<Outcome> m_outcome;
    std::optional<CookInfo> m_cookInfo;
};

#endif // TSUMESTRICTCERTIFIER_H
