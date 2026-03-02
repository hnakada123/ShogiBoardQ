#ifndef TSUMESHOGIGENERATOR_H
#define TSUMESHOGIGENERATOR_H

/// @file tsumeshogigenerator.h
/// @brief 詰将棋局面自動生成オーケストレータの定義

#include "tsumeshogipositiongenerator.h"

#include <QElapsedTimer>
#include <QFutureWatcher>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QList>
#include <memory>

#include "threadtypes.h"

class Usi;
enum class PlayMode;

/**
 * @brief 詰将棋局面自動生成のオーケストレーション
 *
 * ランダム局面生成→エンジン詰み探索→結果フィルタリングのループを制御する。
 * 有効局面発見後は不要駒トリミングを行い、最小限の駒で構成された局面を出力する。
 * Usiインスタンスを直接作成・管理し、signal-drivenループで次々に局面を送信する。
 */
class TsumeshogiGenerator : public QObject
{
    Q_OBJECT

public:
    struct Settings {
        QString enginePath;
        QString engineName;
        int targetMoves = 3;         ///< 目標手数（奇数: 1,3,5,7,...）
        int timeoutMs = 5000;        ///< 1局面あたりの探索時間(ms)
        int maxPositionsToFind = 10; ///< 見つける局面数の上限（0=無制限）
        TsumeshogiPositionGenerator::Settings posGenSettings;
    };

    explicit TsumeshogiGenerator(QObject* parent = nullptr);
    ~TsumeshogiGenerator() override;

    void start(const Settings& settings);
    void stop();
    bool isRunning() const;

signals:
    void positionFound(const QString& sfen, const QStringList& pv);
    void progressUpdated(int tried, int found, qint64 elapsedMs);
    void finished();
    void errorOccurred(const QString& message);

private slots:
    void onCheckmateSolved(const QStringList& pv);
    void onCheckmateNoMate();
    void onCheckmateNotImplemented();
    void onCheckmateUnknown();
    void onSafetyTimeout();
    void onProgressTimerTimeout();
    void onBatchReady();

private:
    /// 状態機械
    enum class Phase { Idle, Searching, Trimming };

    /// 除去候補
    struct TrimCandidate {
        enum Location { Board, Hand };
        Location location = Board;
        int file = -1;           ///< Board時: 筋 (0-8)
        int rank = -1;           ///< Board時: 段 (0-8)
        QChar pieceChar;         ///< SFEN文字 (P/p/k等)
        bool promoted = false;   ///< Board時: 成駒か
    };

    /// SFEN解析結果
    struct ParsedSfen {
        QString cells[9][9];     ///< [rank][file] = "" or "P"/"+R"/"p" etc.
        int senteHand[7] = {};   ///< P,L,N,S,G,B,R の先手持駒数
        int goteHand[7] = {};    ///< P,L,N,S,G,B,R の後手持駒数
    };

    // バッチ生成
    void startBatchGeneration();

    // 検索フェーズ
    void generateAndSendNext();
    void processResult(bool found, const QStringList& pv = {});
    void cleanup();

    // SFEN解析・再構築
    ParsedSfen parseSfen(const QString& sfen) const;
    QString buildSfenFromParsed(const ParsedSfen& parsed) const;

    // 駒種変換
    static int pieceCharToIndex(QChar upper);
    static QChar indexToPieceChar(int idx);

    // トリミング操作
    QList<TrimCandidate> enumerateRemovablePieces(const QString& sfen) const;
    QString removePieceFromSfen(const QString& sfen, const TrimCandidate& candidate) const;

    // トリミングフェーズ制御
    void startTrimmingPhase(const QString& sfen, const QStringList& pv);
    void tryNextTrimCandidate();
    void sendTrimmingCheck(const QString& modifiedSfen);
    void finishTrimmingPhase();

    std::unique_ptr<Usi> m_usi;
    std::unique_ptr<PlayMode> m_playMode;
    TsumeshogiPositionGenerator m_positionGenerator;
    Settings m_settings;

    QString m_currentSfen;       ///< 現在探索中のSFEN文字列
    int m_triedCount = 0;        ///< 探索済み局面数
    int m_foundCount = 0;        ///< 発見局面数
    Phase m_phase = Phase::Idle; ///< 現在のフェーズ

    QElapsedTimer m_elapsedTimer; ///< 全体経過時間
    QTimer m_safetyTimer;         ///< エンジン無応答ガードタイマー
    QTimer m_progressTimer;       ///< プログレス更新タイマー

    // バッチ生成用状態
    QFutureWatcher<QStringList> m_batchWatcher; ///< バッチ生成の非同期監視
    QStringList m_positionQueue;                 ///< 生成済み局面のキュー
    CancelFlag m_cancelFlag;                     ///< バッチ生成のキャンセルフラグ
    bool m_waitingForPositions = false;          ///< キュー空で生成待ちフラグ

    // トリミング用状態
    QString m_trimBaseSfen;                  ///< トリミング元のSFEN
    QStringList m_trimBasePv;                ///< トリミング元のPV
    QList<TrimCandidate> m_trimCandidates; ///< 除去候補リスト
    int m_trimCandidateIndex = 0;            ///< 現在試行中の候補インデックス
    QString m_trimTestSfen;                  ///< 現在テスト中の除去済みSFEN
};

#endif // TSUMESHOGIGENERATOR_H
