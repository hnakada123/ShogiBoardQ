#ifndef P2PENGINEPLAYER_H
#define P2PENGINEPLAYER_H

/// @file p2pengineplayer.h
/// @brief P2P対局で自分の手を指すエンジン（双方が署名した手順から、エンジンに送る局面と持ち時間を作る）

#include <QObject>
#include <QString>

#include "matchtimesource.h"
#include "usimatchhandler.h"

class EngineSlots;
class MatchEngineSeat;

namespace P2p {
class Match;
struct FinalResult;
} // namespace P2p

/**
 * @brief P2P対局で自分の手を指す登録エンジン（枠 E1）
 *
 * エンジンに送る局面は、双方が署名した手順（開始局面からの手の列）。持ち時間は、双方が署名した残り時間と、
 * 今の自分の手番の経過時間から返す（MatchTimeSource）。指し手・投了・入玉宣言・異常はシグナルで知らせるだけで、
 * 対局にどう反映するか（相手へ送る・投了・申し出・中断）は司令塔（P2pGameCoordinator）が決める。
 */
class P2pEnginePlayer : public QObject, public MatchTimeSource
{
    Q_OBJECT
public:
    explicit P2pEnginePlayer(EngineSlots* engines, QObject* parent = nullptr);
    ~P2pEnginePlayer() override;

    /// エンジンを枠 E1 に置いて起動する（前のエンジンは手放す）。初期化の完了は initialized、
    /// 失敗は errorOccurred で、どちらもこの呼び出しから戻った後に届く
    void start(const QString& enginePath, const QString& engineName);
    /// readyok を受け、指し手を頼める
    bool isReady() const;

    /// 対局を始める（局面と持ち時間は、この対局の手順から作る。match は非所有で、手放すまで生きていること）
    void beginMatch(const P2p::Match* match);
    /// 指し手を頼む（同じ局面では1回だけ）。自分の手番で確認待ちの手がないことは、呼ぶ側が確かめる
    void requestMove();
    /// 考えている手をすぐ指させる
    void moveNow();
    /// 考えている指し手を、もう受けない（人が投了したとき）
    void cancelThinking();
    /// 自分の手番で、持ち時間をほぼ使い切ったか（その間に止まったエンジンは、持ち時間の間に指し手を返さなかった
    /// ものとして扱う。時間切れは対局の状態機械が相手に知らせる）
    bool hasRunOutOfTime() const;

    /// 終局をエンジンに知らせて枠を空ける（result が無ければ勝ち負けのない終局）
    void finish(const P2p::FinalResult* result);
    /// 枠を空ける（終局を知らせない。対局が始まらなかったとき）
    void release();

    // --- MatchTimeSource ---
    MatchSearchTiming snapshot(Turn side, bool pondering) const override;

signals:
    /// 初期化（usi〜readyok）が済んだ
    void initialized();
    void moveReady(const UsiMatchMove& move);
    void resigned();
    void declaredWin();
    /// 起動の失敗・異常終了・枠の取り上げ
    void errorOccurred(const QString& message);

private slots:
    void onResigned(Turn side);
    void onDeclaredWin(Turn side);

private:
    Turn mySide() const;

    MatchEngineSeat* m_seat = nullptr;
    const P2p::Match* m_match = nullptr;
    qint64 m_requestedPly = -1;   ///< 指し手を頼んだ局面の手数（同じ局面で2回頼まない）
};

#endif // P2PENGINEPLAYER_H
