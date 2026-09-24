#ifndef TSUMESOLUTIONREPLAY_H
#define TSUMESOLUTIONREPLAY_H

#include "tsumeevaluation.h"
#include <QObject>

class TsumePositionAnalyzer;
class TsumeProgressStore;

/// 対局状態・挑戦履歴を変更せず、検証済みの正解手順を任意の手数で表示する。
class TsumeSolutionReplay : public QObject
{
    Q_OBJECT
public:
    explicit TsumeSolutionReplay(QObject* parent = nullptr);
    void configure(const QString& sfen, const QString& enginePath, TsumeProgressStore* store);
    void seek(int ply, int milliseconds);
    void cancel();
    bool loading() const { return m_loading; }
    bool available() const { return !m_moves.isEmpty(); }
    int currentPly() const { return m_ply; }
    int totalPlies() const { return static_cast<int>(m_moves.size()); }
    int requestedPly() const { return m_requested; }
    QString detail() const { return m_detail; }
    const QStringList& moves() const { return m_moves; }
signals:
    void stateChanged();
    void positionChanged(const QString& sfen, const QString& move);
private slots:
    void evaluationFinished(const TsumeEvaluation& result);
private:
    void displayRequested();
    TsumePositionAnalyzer* m_analyzer = nullptr;
    QString m_sfen;
    QStringList m_moves;
    QStringList m_positions;
    QString m_detail;
    int m_ply = 0;
    int m_requested = 0;
    bool m_loading = false;
};

#endif
