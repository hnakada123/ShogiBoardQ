#ifndef ENGINEANALYSISRUNNER_H
#define ENGINEANALYSISRUNNER_H

/// @file engineanalysisrunner.h
/// @brief 登録エンジンで 1 局面を一定時間解析する実行器（自動化・CLI 用）

#include <QElapsedTimer>
#include <QMap>
#include <QObject>
#include <QString>
#include <QTimer>

#include "usiinfolineparser.h"

class UsiEngineSession;

/**
 * @brief `go infinite` → 一定時間後に `stop` → `bestmove` までを 1 回実行する
 *
 * 読み筋は `lineUpdated` で逐次通知し、完了時に MultiPV ごとの最新行と bestmove を返す。
 */
class EngineAnalysisRunner : public QObject
{
    Q_OBJECT

public:
    struct Request {
        QString enginePath;
        QString engineName;
        QString positionCommand;   ///< "position startpos moves ..." / "position sfen ..."
        int thinkMs = 5000;        ///< 探索時間(ms)
        int multiPv = 1;           ///< 候補手数
    };

    explicit EngineAnalysisRunner(QObject* parent = nullptr);

    /// エンジンを起動して解析を始める。失敗時は false と error
    [[nodiscard]] bool start(const Request& request, QString* error = nullptr);
    /// 早期停止（bestmove を待って finished を発行する）
    void stop();

    /// MultiPV 番号ごとの最新の読み筋
    QMap<int, UsiInfoLine> latestLines() const { return m_latest; }
    qint64 elapsedMs() const { return m_elapsed.isValid() ? m_elapsed.elapsed() : 0; }

signals:
    void lineUpdated(const UsiInfoLine& line);
    void finished(const QString& bestmove, const QString& ponder);
    void errorOccurred(const QString& message);

private slots:
    void onInfoLine(const QString& line);
    void onBestMove();
    void onThinkTimeout();
    void onNoResponse();
    void onSessionError(const QString& message);

private:
    void finish(const QString& bestmove, const QString& ponder);

    UsiEngineSession* m_session = nullptr; ///< QObject parent 所有
    QTimer m_thinkTimer;
    QTimer m_noResponseTimer;
    QElapsedTimer m_elapsed;
    QMap<int, UsiInfoLine> m_latest;
    bool m_done = false;
};

#endif // ENGINEANALYSISRUNNER_H
