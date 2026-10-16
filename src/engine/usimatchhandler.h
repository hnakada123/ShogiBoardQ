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
#include "usiposition.h"
#include "usitimingparams.h"

class ShogiClock;
class UsiProtocolHandler;

/// 対局でエンジンが指した手
struct UsiMatchMove {
    Turn side = Turn::Black;   ///< 指した側
    UsiMove move;              ///< 指し手（成り・不成を含む）
    qint64 elapsedMs = 0;      ///< 考えた時間（go または ponderhit から bestmove まで）
    UsiScore score;            ///< 最善の読み筋の最後の評価値（指した側から見た値）
};

Q_DECLARE_METATYPE(UsiMatchMove)

/**
 * @brief 対局での position/go/bestmove のやり取り（先読みを含む）
 *
 * 手番のたびに時計から持ち時間を読んで go を送り、持ち時間の間に bestmove が届かなければ知らせる。
 * 指した後は、予想した相手の手で先読みを始める。予想が当たれば ponderhit、外れれば stop で回収してから次の go を送る。
 * 先読みの局面はこの部品だけが持つ（待ったなどで局面が変わったら forgetPonder() で捨てる）。
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
    void requestMove(const UsiPosition& position, const UsiTimingParams& timing);
    void cancelAsync();
    /// 先読みの局面を捨てる（待った・中断の後など。次の手番の探索は先読みを外れとして扱う）
    void forgetPonder() { m_ponder = {}; }
    /// 先読みしている局面（予想した相手の手まで。先読みしていなければ空）
    const UsiPosition& ponderPosition() const { return m_ponder; }

signals:
    void moveReady(const UsiMatchMove& move);
    void resigned(Turn side);
    void declaredWin(Turn side);
    /// 持ち時間の間に bestmove が届かなかった
    void timedOut();
    void errorOccurred(const QString& message);

private slots:
    void onBestMoveReceived(const UsiBestMove& result);
    void onSearchTimeout();

private:
    void startAsyncSearch();
    void startPondering(const UsiBestMove& result);
    /// side の持ち時間で探索の時間を決める（先読みは自分の持ち時間を読む）
    SearchTiming searchTiming(Turn side, bool pondering = false) const;
    /// 時計の手番を確定する。時間切れなら false
    bool finishTurn();

    enum class Pending { None, PonderStop, Move };
    Pending m_pending = Pending::None;
    UsiPosition m_position;
    UsiPosition m_ponder;
    UsiTimingParams m_timing;
    QTimer m_responseTimer;

    UsiProtocolHandler* m_protocolHandler = nullptr;
    ShogiClock* m_clock = nullptr;
};

#endif // USIMATCHHANDLER_H
