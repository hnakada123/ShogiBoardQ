#ifndef ENGINEANALYSISRUNNER_H
#define ENGINEANALYSISRUNNER_H

/// @file engineanalysisrunner.h
/// @brief 登録エンジンで局面を順に一定時間ずつ解析する実行器（CLI の analyze・analyze-kifu）

#include <QElapsedTimer>
#include <QList>
#include <QMap>
#include <QObject>
#include <QString>

#include "usiposition.h"
#include "usiinfo.h"
#include "usimessages.h"

class UsiEngineSession;

/**
 * @brief 局面ごとに `go infinite` → 一定時間後に `stop` → `bestmove` を行う
 *
 * エンジンは1回だけ起動し、局面を順に解析する（棋譜全体の解析も同じエンジンで続ける）。読み筋は
 * `lineUpdated` で逐次知らせ、局面ごとに MultiPV ごとの最新行と bestmove を `positionFinished` で返す。
 * 全部の局面を終えるか止めると `finished`。
 */
class EngineAnalysisRunner : public QObject
{
    Q_OBJECT

public:
    struct Request {
        QString enginePath;
        QString engineName;
        QList<UsiPosition> positions;  ///< 解析する局面（この順に解析する）
        int thinkMs = 5000;            ///< 1局面の探索時間(ms)
        int multiPv = 1;               ///< 候補手数
    };

    explicit EngineAnalysisRunner(QObject* parent = nullptr);

    /// エンジンを起動して解析を始める（起動の失敗は戻った後に errorOccurred）
    void start(const Request& request);
    /// 早く止める。解析中の局面は bestmove を待って positionFinished を出し、残りの局面は解析しない
    void stop();

    /// 解析している（最後に解析した）局面の、MultiPV 番号ごとの最新の読み筋
    QMap<int, UsiInfo> latestLines() const { return m_latest; }
    /// 解析している（最後に解析した）局面の経過時間
    qint64 elapsedMs() const { return m_elapsed.isValid() ? m_elapsed.elapsed() : 0; }
    /// stop() で止めたか
    bool isStopRequested() const { return m_stopRequested; }

signals:
    /// index 番目の局面の解析を始めた
    void positionStarted(int index);
    void lineUpdated(const UsiInfo& line);
    void positionFinished(int index, const QString& bestmove, const QString& ponder);
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
    void searchCurrent();
    void finish(bool stopped);
    void fail(const QString& message);

    UsiEngineSession* m_session = nullptr; ///< QObject parent 所有
    QElapsedTimer m_elapsed;
    QMap<int, UsiInfo> m_latest;
    Request m_request;
    int m_index = 0;
    bool m_done = false;
    bool m_stopRequested = false;
};

#endif // ENGINEANALYSISRUNNER_H
