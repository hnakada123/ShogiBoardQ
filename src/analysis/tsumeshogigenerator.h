#ifndef TSUMESHOGIGENERATOR_H
#define TSUMESHOGIGENERATOR_H

/// @file tsumeshogigenerator.h
/// @brief 詰将棋局面の自動生成（候補の待ち行列・問い合わせの中継・結果の集計）

#include "tsumecandidatejob.h"
#include "tsumeshogicandidatescreener.h"
#include "tsumeshogipositiongenerator.h"

#include <QElapsedTimer>
#include <QFutureWatcher>
#include <QObject>
#include <QSet>
#include <QStringList>
#include <QTimer>
#include <memory>

#include "threadtypes.h"

class MateOracle;
class MateQueryLoop;

/**
 * @brief 詰将棋局面の自動生成
 *
 * ワーカースレッドでランダム局面を作って内蔵探索で候補を選び（バッチ）、候補を1つずつ TsumeCandidateJob で
 * 採択か棄却まで進める。エンジンへの問い合わせは MateOracle、問い合わせの順序は MateQueryLoop が受け持つ。
 * このクラスは候補の待ち行列・問い合わせの中継・結果の集計（重複の除外・上限・進捗）だけを行う。
 * 判定できない・中断した候補は採択しない。大域的な最少駒数を保証するものではない。
 */
class TsumeshogiGenerator : public QObject
{
    Q_OBJECT

public:
    struct Settings {
        QString enginePath;
        QString engineName;
        int targetMoves = 3;         ///< 目標手数（奇数: 1,3,5,7,...）
        int timeoutMs = 5000;        ///< 候補探索・各トリミング探索・各余詰検査全体の時間(ms)
        int maxPositionsToFind = 10; ///< 見つける局面数の上限（0=無制限）
        bool allowFinalMoveAlternatives = true; ///< 主手順の最終手の複数解を許容する（最終手余詰）
        TsumeshogiPositionGenerator::Settings posGenSettings;
    };

    explicit TsumeshogiGenerator(QObject* parent = nullptr);
    ~TsumeshogiGenerator() override;

    void start(const Settings& settings);
    void stop();
    bool isRunning() const { return m_running; }

signals:
    void positionFound(const QString& sfen, const QStringList& pv);
    void progressUpdated(int tried, int found, qint64 elapsedMs);
    void searchPhaseStarted();                       ///< 探索フェーズに入った（開始時・候補の検査や除去を終えたとき）
    void trimmingProgress(int candidate, int total); ///< トリミング中の候補進捗（candidate は 1 始まり）
    void verificationProgress(int queries);
    void verificationStatsUpdated(int rejected, int inconclusive);
    void finished();
    void errorOccurred(const QString& message);

private slots:
    void onEngineReady();
    void onQueryStarted();
    void onCandidateFinished();
    void onUnsupported();
    void onNoResponse();
    void onEngineError(const QString& message);
    void onProgressTimerTimeout();
    void onBatchReady();

private:
    friend class TestTsumeshogiVerification;

    /// エンジンとの接続（MateOracle）と問い合わせの順序（MateQueryLoop）を作ってつなぐ
    void createOracle();
    void startBatchGeneration();
    void startNextCandidate();
    /// 候補の棄却の数を集計に足す（増えたら知らせる）
    void reportCandidateStats();
    void publish(const TsumeCandidateJob::Outcome& outcome);
    /// 生成を終える（finished を出す）
    void finish();
    void cleanup();

    Settings m_settings;
    bool m_running = false;

    MateOracle* m_oracle = nullptr;      ///< parent ownership。後始末は cleanup() の deleteLater
    MateQueryLoop* m_loop = nullptr;     ///< m_oracle の子
    std::unique_ptr<TsumeCandidateJob> m_job;   ///< 調べている候補
    int m_jobRejectedReported = 0;       ///< m_job の棄却のうち集計に足した数
    int m_jobInconclusiveReported = 0;

    int m_generatedCount = 0;    ///< 生成して内蔵探索で選別した局面数（候補以外を含む）
    int m_foundCount = 0;        ///< 発見局面数
    QSet<QString> m_foundSfens;  ///< 発見済みSFEN（重複排除用）
    int m_verificationRejected = 0;
    int m_verificationInconclusive = 0;
    int m_screenUnknownCount = 0; ///< 事前選別が時間切れで未確定のままキューに積んだ局面数（終了時にログへ出す）

    QElapsedTimer m_elapsedTimer; ///< 全体経過時間
    QTimer m_progressTimer;       ///< プログレス更新タイマー

    // バッチ生成
    QFutureWatcher<TsumeshogiCandidateScreener::Batch> m_batchWatcher; ///< バッチ生成の非同期監視
    QStringList m_positionQueue;                 ///< 事前選別を通った候補局面のキュー
    CancelFlag m_cancelFlag;                     ///< バッチ生成のキャンセルフラグ
    bool m_waitingForPositions = false;          ///< キュー空で生成待ちフラグ
};

#endif // TSUMESHOGIGENERATOR_H
