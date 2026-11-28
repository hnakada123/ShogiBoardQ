#ifndef TSUMEPOSITIONANALYZER_H
#define TSUMEPOSITIONANALYZER_H

#include "tsumeevaluation.h"
#include <QObject>
#include <QTimer>

class TsumeMateEngine;
class TsumeProgressStore;
class TsumeSolutionSearch;

/// 詰将棋の局面の判定（最短の詰み手数と手順）。判定の方式（内蔵の探索・登録した詰将棋エンジン）を選び、
/// 確定した結果をキャッシュから読み書きする
class TsumePositionAnalyzer : public QObject
{
    Q_OBJECT
public:
    /// 内蔵の探索で初期局面を判定する手数の上限
    static constexpr int kBuiltInMaxPlies = 31;

    explicit TsumePositionAnalyzer(QObject* parent = nullptr);
    ~TsumePositionAnalyzer() override;
    void configure(const QString& enginePath, TsumeProgressStore* store);
    void evaluate(const QString& sfen, int milliseconds, bool requireLine = false);
    void cancel();
    QString engineKey() const { return m_engineKey; }
    QString enginePath() const { return m_enginePath; }
signals:
    void finished(const TsumeEvaluation& result);
private slots:
    void engineFinished(const TsumeEvaluation& result);
    void internalFinished(const TsumeEvaluation& evaluation);
    void deliverCached();
private:
    void complete(const TsumeEvaluation& result);
    TsumeMateEngine* m_engine = nullptr;
    /// 内蔵の判定（反復深化の探索と証明数探索）
    TsumeSolutionSearch* m_search = nullptr;
    TsumeProgressStore* m_store = nullptr;
    QString m_enginePath;
    QString m_engineKey;
    QString m_sfen;
    QString m_id;
    bool m_active = false;
    TsumeEvaluation m_cached;
    QTimer m_cacheTimer;
};

#endif
