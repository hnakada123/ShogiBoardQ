/// @file p2poffercontroller.cpp
/// @brief P2P対局の申し出の箱の実装

#include "p2poffercontroller.h"

#include <QAbstractButton>
#include <QMessageBox>
#include <QTimer>

#include "dialogutils.h"
#include "notificationpresenter.h"
#include "p2pgamecoordinator.h"
#include "p2pmatch.h"

namespace {
/// 尋ねる箱を出してから「承諾」を押せるまでの時間
constexpr int kAcceptDelayMs = 1000;
} // namespace

P2pOfferController::P2pOfferController(P2pGameCoordinator* game, Hooks hooks, QObject* parent)
    : QObject(parent)
    , m_game(game)
    , m_hooks(std::move(hooks))
{
    if (!m_game) return;
    connect(m_game, &P2pGameCoordinator::offerReceived, this, &P2pOfferController::onOfferReceived);
    connect(m_game, &P2pGameCoordinator::offerPending, this, &P2pOfferController::onOfferPending);
    connect(m_game, &P2pGameCoordinator::offerNotSent, this, &P2pOfferController::onOfferNotSent);
    connect(m_game, &P2pGameCoordinator::proposalDeclined, this, &P2pOfferController::onProposalDeclined);
    connect(m_game, &P2pGameCoordinator::statusChanged, this, &P2pOfferController::closeStale);
    connect(m_game, &P2pGameCoordinator::resultAgreed, this, &P2pOfferController::closeStale);
}

P2pOfferController::~P2pOfferController() = default;

QWidget* P2pOfferController::noticeParent() const
{
    return m_hooks.noticeParent ? m_hooks.noticeParent() : nullptr;
}

void P2pOfferController::onOfferReceived(P2p::Cause cause)
{
    // 前の申し出の箱が残っていれば、答えとして扱わずに閉じる
    closeOfferBox();
    const QString question = cause == P2p::Cause::AgreedDraw ? tr("相手が引き分けを申し出ました。承諾しますか？")
                                                              : tr("相手が対局の中断を申し出ました。承諾しますか？");
    m_offerBox = DialogUtils::createConfirmBox(noticeParent(), tr("P2P対局"), question, tr("承諾"));
    m_offerBox->setObjectName(QStringLiteral("p2pOfferBox"));
    connect(m_offerBox, &QMessageBox::finished, this, &P2pOfferController::onOfferAnswered);
    // 出してすぐは「承諾」を押せない。相手が引き分けと中断の申し出を続けて送ると箱が入れ替わり、前の申し出に向けた
    // クリックで、別の申し出を承諾しうる（ボタンの文字は同じ）
    if (QAbstractButton* accept = m_offerBox->button(QMessageBox::Yes)) accept->setEnabled(false);
    QTimer::singleShot(kAcceptDelayMs, this, &P2pOfferController::enableAccept);
    m_offerBox->show();
}

void P2pOfferController::enableAccept()
{
    if (!m_offerBox) return;
    if (QAbstractButton* accept = m_offerBox->button(QMessageBox::Yes)) accept->setEnabled(true);
}

void P2pOfferController::onOfferAnswered(int result)
{
    // 答えるまでに手数が進んだ・対局が終わったなら、対局の状態機械が無効として扱う
    if (m_game) m_game->answerOffer(result == QMessageBox::Yes);
}

void P2pOfferController::closeStale()
{
    const P2p::Match* match = m_game ? m_game->match() : nullptr;
    if (m_offerBox && (!match || !match->hasPendingOffer())) closeOfferBox();
    // 自分の申し出の答えを待つ知らせも、待つものがなくなったら閉じる
    if (m_waitingBox && (!match || !match->offerAwaitingAnswer())) m_waitingBox->close();
}

void P2pOfferController::closeOfferBox()
{
    if (!m_offerBox) return;
    // 閉じると finished が出るので、先に外す（古い箱を閉じたことを、次の申し出への答えにしない）
    disconnect(m_offerBox, &QMessageBox::finished, this, &P2pOfferController::onOfferAnswered);
    m_offerBox->close();
    m_offerBox.clear();
}

void P2pOfferController::onOfferPending(P2p::Cause cause)
{
    // 申し出た側にも、申し出たことと、相手の答えを待っていることを知らせる。相手が答えた・手数が進んだ・対局が
    // 終わったら閉じる（closeStale）。押し直したときは入れ替える
    if (m_noticeBox) m_noticeBox->close();
    const QString text = cause == P2p::Cause::AgreedDraw
        ? tr("相手に引き分けを申し出ました。相手の返事を待っています。")
        : tr("相手に対局の中断を申し出ました。相手の返事を待っています。");
    NotificationPresenter::replaceInformation(
        m_waitingBox, noticeParent(), tr("P2P対局"),
        text + QLatin1Char('\n') + tr("返事の前に手が進むと、申し出は取り消しになります。"), NotificationPresenter::Wait::No);
    if (m_waitingBox) m_waitingBox->setObjectName(QStringLiteral("p2pOfferWaitingBox"));
}

void P2pOfferController::onOfferNotSent()
{
    NotificationPresenter::replaceInformation(
        m_noticeBox, noticeParent(), tr("P2P対局"),
        tr("今は申し出を送れません（開始の手続き中・再接続中・指した手の確認待ち）。少し待ってから、もう一度操作してください。"),
        NotificationPresenter::Wait::No);
}

void P2pOfferController::onProposalDeclined()
{
    // 答えを待つ知らせを閉じて、断られたことを知らせる。断られた後は同じ手数でももう一度申し出られるので、前の知らせが
    // 残っていれば閉じてから出す（同じ知らせを重ねない）
    if (m_waitingBox) m_waitingBox->close();
    NotificationPresenter::replaceInformation(m_noticeBox, noticeParent(), tr("P2P対局"),
                                              tr("相手が申し出を断りました。対局を続けます。"),
                                              NotificationPresenter::Wait::No);
}
