#include "gamesessionfacade.h"

#include "matchcoordinatorwiring.h"

GameSessionFacade::GameSessionFacade(const Deps& deps)
    : m_deps(deps)
{
}

bool GameSessionFacade::initialize()
{
    if (!m_deps.ensureAndGetWiring) {
        return false;
    }

    m_wiring = m_deps.ensureAndGetWiring();
    if (!m_wiring) {
        return false;
    }

    m_wiring->wireConnections();
    return true;
}

MatchCoordinator* GameSessionFacade::match() const
{
    return m_wiring ? m_wiring->match() : nullptr;
}
