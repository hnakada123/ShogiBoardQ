#ifndef ANALYSISJOB_H
#define ANALYSISJOB_H

/// @file analysisjob.h
/// @brief 棋譜解析1回（エンジンの起動から範囲の全局面の解析まで）

#include <QList>
#include <QMetaType>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QStringList>
#include <optional>

#include "pvtext.h"
#include "recordline.h"
#include "usiinfo.h"
#include "usimessages.h"
#include "usiposition.h"

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
    int lineIndex = 0;
    QSet<int> branchPlies;  ///< 分岐がある手の手数（結果の表で分岐の印を付ける）
};

/// 1局面の解析の結果（bestmove の時点で1回だけ作る）
struct PlyAnalysis {
    int ply = -1;
    UsiScore score;       ///< 最善の読み筋（multipv 1）の最後の評価値（その局面の手番側から見た値。定跡は None）
    PvText pv;            ///< 最善の読み筋（解析した局面の上で指した手と、指せなかった語）
    int depth = -1;       ///< 最善の読み筋を出したときの探索深さ（無ければ -1）
    bool book = false;    ///< info が1つも届かずに bestmove が届いた（定跡）
};

Q_DECLARE_METATYPE(PlyAnalysis)

/**
 * @brief 1局面の info を集めて、bestmove で結果（PlyAnalysis）にする（Qt のイベントループを知らない）
 *
 * 読み筋は最善の読み筋（multipv 1）の最後の行から、評価値は bestmove が持つ値（UsiBestMove::score）から取る。
 * 読み筋は、探索した局面の上でここで指す（思考情報の表示の整形には頼らない）。
 */
class PlyAnalysisCollector
{
public:
    void begin(int ply, const UsiPosition& position);
    void addInfo(const UsiInfo& info);
    PlyAnalysis finish(const UsiBestMove& bestMove) const;

private:
    int m_ply = -1;
    UsiPosition m_position;
    bool m_anyInfo = false;
    QStringList m_bestPv;   ///< multipv 1 の最後の読み筋
    int m_bestDepth = -1;   ///< その読み筋の探索深さ
};

/**
 * @brief 棋譜解析1回の入力・進行・エンジンの利用期間を受け持つ
 *
 * 範囲の局面を1つずつ「position + go infinite → 思考時間で stop → bestmove」で解析し、
 * 局面ごとに plyStarted と plyAnalyzed を1回ずつ出す。画面の応答を待たずに進む。
 */
class AnalysisJob : public QObject
{
    Q_OBJECT
public:
    struct Deps {
        Usi* engine = nullptr; ///< 使うエンジン（必須。借用。思考情報・通信ログの表示は持ち主が結ぶ）
    };
    AnalysisJob(AnalysisRequest request, const Deps& deps, QObject* parent = nullptr);
    ~AnalysisJob() override;

    bool start();
    /// 解析をやめる（finished(false) を出す）
    void stop();
    QString error() const { return m_error; }
    const RecordLine& line() const { return m_request.line; }
    Usi* engine() const;
    bool isRunning() const { return m_state == State::Initializing || m_state == State::Analyzing; }

signals:
    /// 局面の解析を始めた
    void plyStarted(int ply);
    /// 局面の解析を終えた
    void plyAnalyzed(const PlyAnalysis& result);
    /// 解析を終えた（completed: 範囲の全局面を解析した）
    void finished(bool completed);
    void errorOccurred(const QString& message);

private slots:
    void onInitialized();
    void onInfo(const UsiInfo& info);
    void onBestMove(const UsiBestMove& result);
    void onError(const QString& message);

private:
    enum class State { Ready, Initializing, Analyzing, Finished };

    void analyzePly(int ply);
    void finish(bool completed);

    AnalysisRequest m_request;
    QList<std::optional<UsiPosition>> m_positions;   ///< 手順の各局面（[0] が開始局面。読めない局面は nullopt）
    QPointer<Usi> m_engine;
    State m_state = State::Ready;
    QString m_error;
    int m_ply = -1;                   ///< 解析している手数
    PlyAnalysisCollector m_collector;
};

#endif // ANALYSISJOB_H
