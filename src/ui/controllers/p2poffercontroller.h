#ifndef P2POFFERCONTROLLER_H
#define P2POFFERCONTROLLER_H

/// @file p2poffercontroller.h
/// @brief P2P対局の合意引き分け・合意中断の申し出の箱（相手の申し出を尋ねる・自分の申し出の答えを待つ・断られた知らせ）

#include <QObject>
#include <QPointer>

#include <functional>

#include "p2pprotocol.h"

class P2pGameCoordinator;
class QMessageBox;
class QTimer;
class QWidget;

/**
 * @brief 申し出の箱の組と、相手の申し出への答え
 *
 * どの箱も待たずに出す（司令塔の知らせの途中でイベントループを回さない。答えるまでの間も盤で指せる）。申し出が
 * 無効になったら（手数が進んだ・結果が決まった・対局が終わった）、尋ねる箱と答えを待つ知らせを閉じる。
 */
class P2pOfferController : public QObject
{
    Q_OBJECT
public:
    struct Hooks {
        /// 箱の親（利用者が使っている側のウィンドウ）
        std::function<QWidget*()> noticeParent;
    };

    /// game は非所有（この部品より長く生きること）
    P2pOfferController(P2pGameCoordinator* game, Hooks hooks, QObject* parent = nullptr);
    ~P2pOfferController() override;

public slots:
    /// 申し出が無効になったなら、相手の申し出を尋ねる箱と、自分の申し出の答えを待つ知らせを閉じる
    void closeStale();

private slots:
    void onOfferReceived(P2p::Cause cause);
    /// 相手の申し出に答えた（result は QMessageBox::Yes なら承諾）
    void onOfferAnswered(int result);
    /// 尋ねる箱の「承諾」を押せるようにする（出してから少し待つ）
    void enableAccept();
    /// 自分の申し出を相手へ送り、答えを待っている（申し出た側にも知らせる）
    void onOfferPending(P2p::Cause cause);
    /// 自分の申し出を送れなかった
    void onOfferNotSent();
    void onProposalDeclined();

private:
    /// 尋ねる箱を、答えとして扱わずに閉じる
    void closeOfferBox();
    QWidget* noticeParent() const;

    QPointer<P2pGameCoordinator> m_game;
    Hooks m_hooks;
    QPointer<QMessageBox> m_offerBox;    ///< 相手の申し出を尋ねる箱（閉じたら消える）
    QPointer<QMessageBox> m_waitingBox;  ///< 自分の申し出の答えを待つ知らせ（待つものがなくなったら閉じる）
    QPointer<QMessageBox> m_noticeBox;   ///< 自分の申し出を断られた・送れなかった知らせ（次の知らせで入れ替える）
    /// 尋ねる箱の「承諾」を押せるようにするまでの時間（箱を出すたびに測り直す。前の箱の時間で、後から出た箱の
    /// 「承諾」を押せるようにしない。監査 2026-11-21 S6）
    QTimer* m_acceptTimer = nullptr;
};

#endif // P2POFFERCONTROLLER_H
