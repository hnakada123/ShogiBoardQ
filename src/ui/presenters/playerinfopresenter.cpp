/// @file playerinfopresenter.cpp
/// @brief 盤の対局者名の表示の実装

#include "playerinfopresenter.h"

#include "appactivitystate.h"
#include "kifudocument.h"
#include "logcategories.h"
#include "shogiview.h"

PlayerInfoPresenter::PlayerInfoPresenter(const Deps& deps, QObject* parent)
    : QObject(parent)
    , m_deps(deps)
{
    Q_ASSERT(m_deps.document && m_deps.activity);
    syncBoardNamesWithGameInfo();
}

PlayerInfoPresenter::~PlayerInfoPresenter() = default;

void PlayerInfoPresenter::showMatchPlayers()
{
    if (m_deps.activity->is(AppActivityState::Activity::DuringGame)) showPlayers(m_deps.document->players());
    else syncBoardNamesWithGameInfo();
}

void PlayerInfoPresenter::showPlayers(const MatchPlayers& players)
{
    qCDebug(lcUi).noquote() << "showPlayers: black=" << players.name(Turn::Black)
                            << " white=" << players.name(Turn::White);
    setBoardNames(players.name(Turn::Black), players.name(Turn::White));
}

void PlayerInfoPresenter::setBoardNames(const QString& black, const QString& white)
{
    if (!m_deps.shogiView) return;
    m_deps.shogiView->chrome()->card(Turn::Black)->setName(black.isEmpty() ? tr("先手") : black);
    m_deps.shogiView->chrome()->card(Turn::White)->setName(white.isEmpty() ? tr("後手") : white);
}

void PlayerInfoPresenter::syncBoardNamesWithGameInfo()
{
    // 先手・後手が無ければ駒落ちの下手・上手、それも無ければ省略名
    setBoardNames(m_deps.document->headerPlayerName(Turn::Black), m_deps.document->headerPlayerName(Turn::White));
}

