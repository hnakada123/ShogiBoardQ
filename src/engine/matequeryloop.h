#ifndef MATEQUERYLOOP_H
#define MATEQUERYLOOP_H

/// @file matequeryloop.h
/// @brief 引き出し型の問い合わせ元（次に調べる局面を返す）を MateOracle で回す

#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QTimer>
#include <optional>

#include "mateoracle.h"

/// 詰みの問い合わせ1回
struct MateQuery {
    QString sfen;      ///< 攻方手番の局面
    int limitMs = 0;   ///< go mate の制限時間
};

/**
 * @brief 引き出し型の問い合わせ元（Qt のイベントループを知らない）
 *
 * nextQuery() で次の問い合わせを返し、submit() で答えを受け取る。isDone() になるまで繰り返す。
 * nextQuery() が空を返して isDone() でなければ、イベントループに戻ってから呼び直す（長い計算を分けるため）。
 */
class MateQuerySource
{
public:
    virtual ~MateQuerySource() = default;
    virtual std::optional<MateQuery> nextQuery() = 0;
    /// 答えと、その問い合わせにかかった時間（総予算の計算に使う）
    virtual void submit(const MateAnswer& answer, qint64 elapsedMs) = 0;
    virtual bool isDone() const = 0;
};

/**
 * @brief 問い合わせ元を MateOracle で最後まで回す
 *
 * エンジンが go mate に対応していなければ unsupported で止まる。エンジンの異常（errorOccurred・timedOut）は
 * MateOracle の持ち主が受ける。
 */
class MateQueryLoop : public QObject
{
    Q_OBJECT

public:
    explicit MateQueryLoop(MateOracle* oracle, QObject* parent = nullptr);

    /// source を回し始める（非所有。finished・unsupported・cancel() まで生かしておく）
    void run(MateQuerySource* source);
    /// 回すのをやめる（答えを待っている問い合わせは捨てる）
    void cancel();
    bool isRunning() const { return m_source != nullptr; }

signals:
    /// 問い合わせを送った（source の状態は送った直後のもの）
    void queryStarted(const MateQuery& query);
    void finished();
    void unsupported();

private slots:
    void step();
    void onAnswered(const MateAnswer& answer);

private:
    MateOracle* m_oracle = nullptr;
    MateQuerySource* m_source = nullptr;
    QTimer m_stepTimer;
    QElapsedTimer m_queryTimer;
    bool m_awaiting = false;
};

#endif // MATEQUERYLOOP_H
