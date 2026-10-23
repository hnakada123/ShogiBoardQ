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
    const KifuDocument* document = m_deps.document;
    connect(document, &KifuDocument::playersChanged, this, &PlayerInfoPresenter::refresh);
    connect(document, &KifuDocument::headerReset, this, &PlayerInfoPresenter::refresh);
    connect(document, &KifuDocument::headerEdited, this, &PlayerInfoPresenter::refresh);
    connect(document, &KifuDocument::headerValueSet, this, &PlayerInfoPresenter::refresh);
    connect(m_deps.activity, &AppActivityState::changed, this, &PlayerInfoPresenter::refresh);
    refresh();
}

PlayerInfoPresenter::~PlayerInfoPresenter() = default;

void PlayerInfoPresenter::refresh()
{
    if (m_deps.activity->is(AppActivityState::Activity::DuringGame)) {
        const MatchPlayers& players = m_deps.document->players();
        qCDebug(lcUi).noquote() << "board names: black=" << players.name(Turn::Black)
                                << " white=" << players.name(Turn::White);
        setBoardNames(players.name(Turn::Black), players.name(Turn::White));
        return;
    }
    // 先手・後手が無ければ駒落ちの下手・上手、それも無ければ省略名
    setBoardNames(m_deps.document->headerPlayerName(Turn::Black), m_deps.document->headerPlayerName(Turn::White));
}

void PlayerInfoPresenter::setBoardNames(const QString& black, const QString& white)
{
    if (!m_deps.shogiView) return;
    m_deps.shogiView->chrome()->card(Turn::Black)->setName(black.isEmpty() ? tr("先手") : black);
    m_deps.shogiView->chrome()->card(Turn::White)->setName(white.isEmpty() ? tr("後手") : white);
}
