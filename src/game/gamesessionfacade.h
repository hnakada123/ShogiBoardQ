#ifndef GAMESESSIONFACADE_H
#define GAMESESSIONFACADE_H

#include <functional>

class MatchCoordinator;
class MatchCoordinatorWiring;
class GameStartCoordinator;

class GameSessionFacade
{
public:
    struct Deps {
        std::function<MatchCoordinatorWiring*()> ensureAndGetWiring;
    };

    explicit GameSessionFacade(const Deps& deps);

    bool initialize();
    MatchCoordinator* match() const;
    GameStartCoordinator* gameStartCoordinator() const;

private:
    Deps m_deps;
    MatchCoordinatorWiring* m_wiring = nullptr;
};

#endif // GAMESESSIONFACADE_H
