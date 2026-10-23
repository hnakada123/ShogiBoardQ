#ifndef USIMATCHHANDLER_H
#define USIMATCHHANDLER_H

/// @file usimatchhandler.h
/// @brief 対局での position/go/bestmove のやり取り（先読みを含む）

#include <QElapsedTimer>
#include <QObject>
#include <QTimer>
#include <QString>

#include "matchtimesource.h"
#include "usigotime.h"
#include "usimessages.h"
#include "usimove.h"
#include "usiposition.h"

class UsiEngineSession;
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
 * 手番のたびに時間の出どころ（MatchTimeSource。ローカル対局は対局時計、通信対局はサーバーの残り時間）から
 * 持ち時間を読んで go を送り、持ち時間の間に bestmove が届かなければ知らせる。
 * 指した後は、予想した相手の手で先読みを始める。予想が当たれば ponderhit で続け、外れれば次の探索を頼む
 * （探索の部品 UsiEngineSession が stop で先読みを回収し、その結果を捨ててから go を送る）。
 * 先読みの局面はこの部品だけが持つ（待ったなどで局面が変わったら forgetPonder() で捨てる）。
 * 定跡手（requestBookMove）はエンジンに探索させず、待つ時間の後で同じ moveReady で知らせる。
 * 結果（指し手・投了・宣言勝ち）には指した側を添えて知らせる。
 */
class UsiMatchHandler : public QObject
{
    Q_OBJECT

public:
    using SearchTiming = MatchSearchTiming;

    explicit UsiMatchHandler(UsiEngineSession* session, QObject* parent = nullptr);

    /// 持ち時間を読む出どころ（非所有。無ければ時間の決まりのない対局として扱う）
    void setTimeSource(MatchTimeSource* source) { m_timeSource = source; }

    /// 手番の探索を頼む。先読み中なら当たり・外れを判断する
    void requestMove(const UsiPosition& position);
    /**
     * @brief 手番で定跡手を指す（エンジンには探索させない）
     *
     * 先読み中なら止める（返ってくる bestmove は捨てる）。delayMs 待ってから手番の時間を
     * 精算し、moveReady で知らせる（評価値は無し）。持ち時間が待つ時間より短ければ、時間切れにならないよう縮める。
     * 指した後は先読みしない。
     */
    void requestBookMove(const UsiPosition& position, const UsiMove& move, int delayMs);
    /// 定跡手を待っていれば、すぐ指す（「すぐ指させる」）。待っていなければ false
    bool playBookMoveNow();
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
    /// 定跡手の待つ時間が過ぎた
    void onBookMoveTimer();

private:
    void startAsyncSearch();
    void startPondering(const UsiBestMove& result);
    /// side の持ち時間で探索の時間を決める（先読みは自分の持ち時間を読む）
    SearchTiming searchTiming(Turn side, bool pondering = false) const;
    /// 手番の時間を精算する。時間切れなら false
    bool finishTurn();

    enum class Pending { None, Move, BookMove };
    Pending m_pending = Pending::None;
    UsiPosition m_position;
    UsiPosition m_ponder;
    QTimer m_responseTimer;
    UsiMove m_bookMove;            ///< 待っている定跡手
    QTimer m_bookTimer;            ///< 定跡手を指すまでの待ち
    QElapsedTimer m_bookElapsed;   ///< 定跡手を頼まれてからの時間（考えた時間として知らせる）

    UsiEngineSession* m_session = nullptr;           ///< 非所有（探索の依頼と切り替え）
    UsiProtocolHandler* m_protocolHandler = nullptr; ///< 非所有（m_session の状態機械。先読みの当たりの判断）
    MatchTimeSource* m_timeSource = nullptr;
    FixedTimeSource m_noTime;   ///< 出どころが無いとき（時間の決まりなし）
};

#endif // USIMATCHHANDLER_H
