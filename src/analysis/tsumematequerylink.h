#ifndef TSUMEMATEQUERYLINK_H
#define TSUMEMATEQUERYLINK_H

/// @file tsumematequerylink.h
/// @brief 逆算生成が詰将棋エンジンへ問い合わせる窓口（起動・問い合わせ・応答しないときの再起動）

#include "mateoracle.h"

#include <QElapsedTimer>
#include <QObject>
#include <QString>

/**
 * @brief 詰将棋エンジン1台との接続と、1件ずつの詰みの問い合わせ
 *
 * MateOracle を作り直しながら使う: 起動（最短手数の確認に要るオプションを付ける）、問い合わせ、
 * 期限までに答えないエンジンの再起動（続けて応答しないときはあきらめる）。
 * 答えで何をするか・答えを覚えておくかは使う側（TsumeRetrogradeGenerator）が決める。
 */
class TsumeMateQueryLink : public QObject
{
    Q_OBJECT

public:
    struct Options {
        QString enginePath;
        QString engineName;
        int hashMb = 0;    ///< USI_Hash の上書き（0 は登録設定のまま）
        int threads = 0;   ///< Threads の上書き（0 は登録設定のまま）
    };

    /// 問い合わせを送った結果
    enum class AskResult {
        Unavailable,  ///< 送る前にやめた（起動していない・局面を読めない）
        Sent,         ///< 送った（答えは answered か timedOut）
        Refused,      ///< 送ろうとしたが、エンジンが受け付けなかった
    };

    explicit TsumeMateQueryLink(QObject* parent = nullptr);
    ~TsumeMateQueryLink() override;

    /// 新しい生成のためにエンジンを起動する（起動していれば止めてから。完了は ready）
    void start(const Options& options);
    /// エンジンを止める（以後の応答は知らせない）
    void shutdown();
    /// 期限までに答えなかったエンジンを起動し直す。続けて応答しない回数が上限を超えたら、起動せず false
    bool restartForRecovery(int restartCount);

    bool isReady() const { return m_ready; }
    bool isAwaitingAnswer() const { return m_awaitingAnswer; }

    /// sfen の詰みを問い合わせる。strictInfinite なら go mate infinite で自然に終わるのを期限まで待ち、
    /// そうでなければ制限時間つきで問い合わせる。送る直前に aboutToAsk を出す
    AskResult ask(const QString& sfen, int limitMs, bool strictInfinite);
    /// 最後に送った問い合わせ
    const QString& lastQuerySfen() const { return m_lastQuerySfen; }
    bool lastQueryWasStrict() const { return m_lastQueryWasStrict; }
    /// 最後に送った問い合わせからの時間（送っていなければ 0）
    qint64 lastQueryElapsedMs() const { return m_queryTimer.isValid() ? m_queryTimer.elapsed() : 0; }

    /// 制限時間つきの問い合わせでエンジンの置換表が汚れていれば、新しい対局の合図で捨てさせる
    /// （厳密証明に、打ち切った探索の結果を持ち越さない）
    void resetHashIfDirty();

signals:
    void ready();
    /// 問い合わせを送る直前（問い合わせの数を数える）
    void aboutToAsk();
    /// 答え（go mate に対応していないエンジンの答えは unsupported で知らせる）
    void answered(const MateAnswer& answer, qint64 elapsedMs);
    void unsupported();
    /// 期限までに答えなかった（エンジンが固まっている）
    void timedOut();
    void errorOccurred(const QString& message);

private slots:
    void onOracleReady();
    void onOracleAnswered(const MateAnswer& answer);
    void onOracleTimedOut();

private:
    /// MateOracle を作り直して起動する（最初の起動と、応答しないときの再起動で共通）
    void launch();

    Options m_options;
    MateOracle* m_oracle = nullptr;   ///< 起動のたびに作り直す（QObject parent 所有）
    bool m_ready = false;
    bool m_awaitingAnswer = false;
    bool m_lastQueryWasStrict = false;
    bool m_hashDirty = false;
    int m_consecutiveRecoveries = 0;
    QString m_lastQuerySfen;
    QElapsedTimer m_queryTimer;
};

#endif // TSUMEMATEQUERYLINK_H
