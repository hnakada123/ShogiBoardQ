#ifndef ENGINEANALYSISRUNNER_H
#define ENGINEANALYSISRUNNER_H

/// @file engineanalysisrunner.h
/// @brief 局面の列を1つのエンジンで順に一定時間ずつ解析する実行器（GUI の棋譜解析と CLI の analyze・analyze-kifu で共通）

#include <QElapsedTimer>
#include <QList>
#include <QMap>
#include <QMetaType>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <optional>

#include "pvtext.h"
#include "usiinfo.h"
#include "usimessages.h"
#include "usiposition.h"

class UsiEngineSession;

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
 * @brief 局面ごとに `go infinite` → 一定時間後に `stop` → `bestmove` を行う
 *
 * 借りたエンジン（UsiEngineSession）を1回だけ起動し、局面を順に解析する（棋譜全体の解析も同じエンジンで続ける）。
 * 読み筋は `lineUpdated` で逐次知らせ、局面ごとに結果（PlyAnalysis）と bestmove を `positionFinished` で返す。
 * 全部の局面を終えるか止めると、エンジンを終えて `finished`。候補手の数と、stop の後に bestmove を待つ上限は
 * 依頼で決める（候補手の数は棋譜解析は送らず、CLI は送る。待つ上限はどちらも kDefaultReplyTimeoutMs）。
 */
class EngineAnalysisRunner : public QObject
{
    Q_OBJECT

public:
    /// stop の後に bestmove を待つ上限の既定値
    static constexpr int kDefaultReplyTimeoutMs = 10000;

    struct Request {
        QString enginePath;
        QString engineName;
        /// 解析する局面（この順に解析する。nullopt の局面はエンジンに送らずに飛ばす）
        QList<std::optional<UsiPosition>> positions;
        int firstPly = 0;      ///< positions[0] の手数（結果と知らせの手数は firstPly + 添字）
        int thinkMs = 5000;    ///< 1局面の探索時間(ms)
        int multiPv = 0;       ///< 候補手数（0 は送らず、エンジンの設定のまま）
        /// stop の後に bestmove を待つ上限（0 は待ち続ける）。過ぎたら errorOccurred
        int replyTimeoutMs = kDefaultReplyTimeoutMs;
    };

    /// session は借りる（起動・探索・終了はこの実行器が行う。思考情報・通信ログの表示は session の持ち主が結ぶ）
    explicit EngineAnalysisRunner(UsiEngineSession* session, QObject* parent = nullptr);
    ~EngineAnalysisRunner() override;

    /// エンジンを起動して解析を始める（起動の失敗は戻った後に errorOccurred）
    void start(const Request& request);
    /// 早く止める。解析中の局面は bestmove を待って positionFinished を出し、残りの局面は解析しない
    void stop();
    /// すぐやめる。解析中の局面の結果は待たずに捨て、エンジンを終えて finished(true)
    void cancel();
    /// 起動してから終えるまで
    bool isRunning() const { return m_state == State::Initializing || m_state == State::Analyzing; }

    /// 解析している（最後に解析した）局面の、MultiPV 番号ごとの最新の読み筋
    QMap<int, UsiInfo> latestLines() const { return m_latest; }
    /// 解析している（最後に解析した）局面の経過時間
    qint64 elapsedMs() const { return m_elapsed.isValid() ? m_elapsed.elapsed() : 0; }
    /// stop()・cancel() で止めたか
    bool isStopRequested() const { return m_stopRequested; }
    /// stop の後に bestmove を待つ上限を過ぎて打ち切ったか（errorOccurred の理由。文は CLI 向けの英語なので、
    /// GUI はこれを見て訳した文を出す）
    bool isTimedOut() const { return m_timedOut; }

signals:
    /// ply 手目の局面の解析を始めた
    void positionStarted(int ply);
    void lineUpdated(const UsiInfo& line);
    /// 局面の解析を終えた（result.ply の局面。bestMove はエンジンの答え）
    void positionFinished(const PlyAnalysis& result, const UsiBestMove& bestMove);
    /// 全部の局面を終えた・止めた（stopped）。エラーのときは出さない
    void finished(bool stopped);
    void errorOccurred(const QString& message);

private slots:
    void onSessionReady();
    void onInfo(const UsiInfo& info);
    void onBestMove(const UsiBestMove& result);
    void onNoResponse();
    void onSessionError(const QString& message);

private:
    enum class State { Ready, Initializing, Analyzing, Finished };

    /// index 番目以降で最初に読める局面の解析を始める（無ければ終える）
    void analyzeFrom(int index);
    void finish(bool stopped);
    void fail(const QString& message);

    QPointer<UsiEngineSession> m_session;  ///< 借りたエンジン
    State m_state = State::Ready;
    QElapsedTimer m_elapsed;
    QMap<int, UsiInfo> m_latest;
    PlyAnalysisCollector m_collector;
    Request m_request;
    int m_index = 0;                       ///< 解析している局面の添字
    bool m_stopRequested = false;
    bool m_timedOut = false;
};

#endif // ENGINEANALYSISRUNNER_H
