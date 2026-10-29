#ifndef KIFUANALYSISSESSION_H
#define KIFUANALYSISSESSION_H

/// @file kifuanalysissession.h
/// @brief 棋譜解析の実行（エンジンの枠と動作状態のリース・手順の局面の列の解析）

#include <QObject>
#include <QSet>
#include <QString>
#include <memory>

#include "appactivitystate.h"
#include "engineanalysisrunner.h"
#include "engineslots.h"
#include "recordline.h"

class Usi;

/// 棋譜解析の依頼
struct AnalysisRequest {
    struct Options {
        int startPly = 0;       ///< 解析を始める手数（0以上）
        int endPly = -1;        ///< 解析を終える手数（-1 は末尾まで）
        int movetimeMs = 1000;  ///< 1局面の思考時間（ms）
    };
    RecordLine line;
    Options options;
    QString enginePath;
    QString engineName;
    /// 解析する手順の末尾の手（KifuBranchTree のノード ID。結果の行・グラフの点から棋譜を移すときに使う。-1 は移さない）
    int lineEndNodeId = -1;
    QSet<int> branchPlies;  ///< 分岐がある手の手数（結果の表で分岐の印を付ける）
};

/**
 * @brief 棋譜解析を画面と独立に進める（検討・詰み探索の AnalysisSession と同じ形）
 *
 * 枠 E1 のエンジンを借り（EngineLease）、解析中は動作状態を「棋譜解析中」にする（ActivityLease）。
 * 手順の局面は RecordLine::usiPositions で作り、局面の列の実行器（EngineAnalysisRunner。CLI の analyze-kifu と
 * 同じ）で1つずつ解析して、局面ごとに plyStarted と plyAnalyzed を1回ずつ出す。stop の後に bestmove を返さない
 * エンジンは CLI と同じ時間（EngineAnalysisRunner::kDefaultReplyTimeoutMs）で打ち切り、errorOccurred で知らせる。ほかの処理がエンジンの枠や
 * 動作状態を取ったら解析をやめる。画面（結果の表・評価値グラフ・棋譜の移動）は KifuAnalysisController が受け持つ。
 */
class KifuAnalysisSession : public QObject
{
    Q_OBJECT

public:
    struct Deps {
        EngineSlots* engines = nullptr;          ///< 解析のエンジンを置く枠 E1（表示は枠を結ぶ側が行う。必須）
        AppActivityState* activity = nullptr;    ///< 解析中の動作状態（必須）
        Usi* engine = nullptr;                   ///< 使うエンジン（テスト用。無ければ解析ごとに枠 E1 に作る）
    };

    explicit KifuAnalysisSession(const Deps& deps, QObject* parent = nullptr);
    ~KifuAnalysisSession() override;

    /**
     * @brief 解析を始める（前の解析は止める）
     *
     * 範囲は手順の手数に収める。手順が空・エンジンを用意できなければ false。エンジンの起動の失敗は、
     * 戻った後に errorOccurred で届く
     */
    bool start(AnalysisRequest request);
    /// 解析をやめる（解析中の局面の結果は待たずに捨てる。finished(false)）
    void stop();
    bool isRunning() const { return m_runner && m_runner->isRunning(); }
    /// 解析している（最後に解析した）依頼（範囲は手順に収めた値）
    const AnalysisRequest& request() const { return m_request; }

signals:
    /// 局面の解析を始めた
    void plyStarted(int ply);
    /// 局面の解析を終えた
    void plyAnalyzed(const PlyAnalysis& result);
    /// 解析を終えた（completed: 範囲の全局面を解析した。止めた・エラーなら false）。動作状態は待機に戻した後
    void finished(bool completed);
    /// エンジンのエラーで解析をやめた（finished(false) の後）
    void errorOccurred(const QString& message);

private slots:
    void onPositionFinished(const PlyAnalysis& result, const UsiBestMove& bestMove);
    void onRunnerFinished(bool stopped);
    void onRunnerError(const QString& message);
    /// ほかの処理（検討・対局など）がエンジンの枠を取った
    void onEngineRevoked(QObject* owner);
    /// ほかの部品が別の動作（検討・対局など）を始めた
    void onActivityRevoked(QObject* owner);

private:
    /// 解析に使うエンジン（テスト用のエンジンか、枠 E1 から借りたエンジン）
    Usi* engine() const;
    /// 思考情報の表示を消してエンジンを終え、動作状態を待機に戻して finished を出す
    void finishRun(bool completed);

    Deps m_deps;
    AnalysisRequest m_request;
    EngineLease m_lease;                        ///< 枠 E1 のエンジンを使う権利（テスト用のエンジンを渡されたときは使わない）
    ActivityLease m_activityLease;              ///< 解析中の動作状態
    std::unique_ptr<EngineAnalysisRunner> m_runner;
};

#endif // KIFUANALYSISSESSION_H
