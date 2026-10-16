#ifndef TSUMEMATEENGINE_H
#define TSUMEMATEENGINE_H

/// @file tsumemateengine.h
/// @brief 最短手数を確認できる詰み探索エンジン（KomoringHeights）で詰みを判定するクラスの定義

#include "tsumeevaluation.h"
#include <QObject>

#include "usimessages.h"
#include <QStringList>

class UsiEngineSession;

/**
 * @brief 攻方手番の go mate のみを実行する。中断時の不完全な最短探索結果は採用しない。
 *
 * エンジンとの接続は UsiEngineSession（ほかの機能と同じ USI の手順）で行い、登録済みエンジンの
 * 保存済みの設定（スレッド数・ハッシュなど）を送る。最短手数の確認に必要なオプションだけは、
 * この接続に限って上書きする（登録済みの設定は変えない）。
 */
class TsumeMateEngine : public QObject
{
    Q_OBJECT
public:
    explicit TsumeMateEngine(QObject* parent = nullptr);
    ~TsumeMateEngine() override;
    void setExecutable(const QString& path);
    void search(const QString& sfen, int milliseconds);
    void cancel();

    /// 最短手数の確認に必要で、この接続に限って指定するオプション
    static QList<QPair<QString, QString>> requiredOptions();

signals:
    void finished(const TsumeEvaluation& result);

private slots:
    void onReady();
    void onSessionError(const QString& message);
    void onCheckmate(const UsiCheckmate& result);
    void timedOut();

private:
    enum class Phase { Off, Starting, Idle, Searching };
    void beginSearch();
    void finishSearch(const TsumeEvaluation& result);
    void fail(const QString& message);

    QString m_executable;
    QString m_sfen;
    UsiEngineSession* m_session = nullptr;  ///< QObject parent 所有
    int m_milliseconds = 5000;
    Phase m_phase = Phase::Off;
};

#endif
