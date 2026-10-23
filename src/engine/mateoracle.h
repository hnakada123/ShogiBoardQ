#ifndef MATEORACLE_H
#define MATEORACLE_H

/// @file mateoracle.h
/// @brief エンジンへの詰みの問い合わせ（go mate）の共通部品

#include <QMetaType>
#include <QObject>
#include <QString>
#include <QStringList>

#include "usimessages.h"
#include "usiposition.h"

class UsiEngineSession;

/// 詰みの問い合わせの答え
struct MateAnswer {
    enum class Kind {
        Mate,          ///< 詰む（pv は詰み手順）
        NoMate,        ///< 詰まない
        Unknown,       ///< 判定できなかった（時間切れ・打ち切った探索の結果）
        Unsupported,   ///< エンジンが go mate に対応していない
    };
    Kind kind = Kind::Unknown;
    QStringList pv;

    /**
     * @brief エンジンの checkmate 応答を答えにする（変換はここだけ）
     *
     * stop を送った後の応答は、打ち切られた探索の途中の結果かもしれないので Unknown にする。
     */
    static MateAnswer fromCheckmate(const UsiCheckmate& result);
};

Q_DECLARE_METATYPE(MateAnswer)

/**
 * @brief エンジン1台に詰みを問い合わせる
 *
 * UsiEngineSession を所有し、ask() に answered() で答える。打ち切りの方針（制限時間＋余裕で stop を送り、
 * その後も答えなければ timedOut）は UsiSearchRequest::mate の1か所で決める。
 * 起動中に頼んだ問い合わせは、初期化が終わってから送る。答えを待つ間に次を頼むと、前の問い合わせは
 * エンジンの側で止めて答えを捨て、次の問い合わせにだけ答える。
 */
class MateOracle : public QObject
{
    Q_OBJECT

public:
    explicit MateOracle(QObject* parent = nullptr);
    ~MateOracle() override;

    /// 起動を頼む（完了は ready、失敗は戻った後に errorOccurred）
    void start(const QString& enginePath, const QString& engineName);
    /// エンジンを止める（以後の応答は知らせない）
    void quit();
    /// 起動して初期化を終えたか
    bool isReady() const;
    /// 問い合わせの答えを待っているか
    bool isAsking() const { return m_asking; }

    /// 詰みを問い合わせる。limitMs が 0 以下なら go mate infinite で、打ち切らない。
    /// 起動していない・局面が空なら問い合わせず false
    bool ask(const UsiPosition& position, int limitMs);
    /// go mate infinite で問い合わせ、deadlineMs までに答えなければ stop を送らずに timedOut を知らせる
    bool askWithin(const UsiPosition& position, int deadlineMs);
    /// 問い合わせを打ち切る（答えは Unknown で届く）
    void stop();

    /// エンジンとの接続（オプションの指定・エンジンが報告した値の確認）
    UsiEngineSession* session() const { return m_session; }

signals:
    void ready();
    void answered(const MateAnswer& answer);
    /// stop にも期限にも答えなかった（エンジンが固まっている）
    void timedOut();
    void errorOccurred(const QString& message);

private slots:
    void onReady();
    void onCheckmate(const UsiCheckmate& result);
    void onTimedOut();
    void onError(const QString& message);

private:
    UsiEngineSession* m_session = nullptr;   ///< QObject parent 所有
    bool m_started = false;
    bool m_ready = false;
    bool m_asking = false;
};

#endif // MATEORACLE_H
