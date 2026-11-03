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

QString PlayerInfoPresenter::boardName(const KifuDocument& document, const AppActivityState& activity, Turn side)
{
    const QString name = activity.is(AppActivityState::Activity::DuringGame) ? document.players().name(side)
                                                                             : document.headerPlayerName(side);
    if (!name.isEmpty()) return name;
    return side == Turn::Black ? tr("先手") : tr("後手");
}

void PlayerInfoPresenter::refresh()
{
    if (!m_deps.shogiView) return;
    for (const Turn side : {Turn::Black, Turn::White}) {
        const QString name = boardName(*m_deps.document, *m_deps.activity, side);
        qCDebug(lcUi).noquote() << "board name:" << (side == Turn::Black ? "black" : "white") << name;
        m_deps.shogiView->chrome()->card(side)->setName(name);
    }
}
