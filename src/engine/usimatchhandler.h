#ifndef USIMATCHHANDLER_H
#define USIMATCHHANDLER_H

/// @file usimatchhandler.h
/// @brief 対局での position/go/bestmove のやり取り（先読みを含む）

#include <QObject>
#include <QTimer>
#include <QString>

#include "usigotime.h"
#include "usimessages.h"
#include "usimove.h"
#include "usitimingparams.h"

class ShogiClock;
class UsiProtocolHandler;

/// 対局でエンジンが指した手
struct UsiMatchMove {
    Turn side = Turn::Black;   ///< 指した側
    UsiMove move;              ///< 指し手（成り・不成を含む）
    QString position;          ///< 指した後の局面の position コマンド
    QString ponderPosition;    ///< 先読みを始めた局面（予想した相手の手まで。先読みしなければ空）
    qint64 elapsedMs = 0;      ///< 考えた時間（go または ponderhit から bestmove まで）
    UsiScore score;            ///< 最善の読み筋の最後の評価値（指した側から見た値）
};

Q_DECLARE_METATYPE(UsiMatchMove)

/**
 * @brief 対局での position/go/bestmove のやり取り（先読みを含む）
 *
 * 手番のたびに時計から持ち時間を読んで go を送り、持ち時間の間に bestmove が届かなければ知らせる。
 * 指した後は、予想した相手の手で先読みを始める。予想が当たれば ponderhit、外れれば stop で回収してから次の go を送る。
 * 結果（指し手・投了・宣言勝ち）には指した側を添えて知らせる。
 */
class UsiMatchHandler : public QObject
{
    Q_OBJECT

public:
    struct SearchTiming {
        UsiGoTimeInput input;
        int timeoutMs = 30 * 60 * 1000;
        bool expired = false;
    };

    explicit UsiMatchHandler(UsiProtocolHandler* protocolHandler, QObject* parent = nullptr);

    /// 持ち時間を読む対局時計（無ければ requestMove の timing をそのまま使う）
    void setClock(ShogiClock* clock) { m_clock = clock; }

    /// 手番の探索を頼む。先読み中なら当たり・外れを判断する
    void requestMove(const QString& position, const QString& ponder, const UsiTimingParams& timing);
    void cancelAsync();

signals:
    void moveReady(const UsiMatchMove& move);
    void resigned(Turn side);
    void declaredWin(Turn side);
    /// 持ち時間の間に bestmove が届かなかった
    void timedOut();
    void errorOccurred(const QString& message);
    /// 読み筋を表示する局面に至った手が変わった（USI 形式）
    void lastMoveChanged(const QString& move);

private slots:
    void onBestMoveReceived(const UsiBestMove& result);
    void onSearchTimeout();

private:
    void startAsyncSearch();
    void startPondering(const UsiBestMove& result);
    SearchTiming searchTiming(bool pondering = false) const;
    /// 時計の手番を確定する。時間切れなら false
    bool finishTurn();

    enum class Pending { None, PonderStop, Move };
    Pending m_pending = Pending::None;
    QString m_position;
    QString m_ponder;
    UsiTimingParams m_timing;
    Turn m_sideToMove = Turn::Black;   ///< 手番の探索の手番（m_position の局面から一度だけ求める）
    QTimer m_responseTimer;

    UsiProtocolHandler* m_protocolHandler = nullptr;
    ShogiClock* m_clock = nullptr;
};

#endif // USIMATCHHANDLER_H
