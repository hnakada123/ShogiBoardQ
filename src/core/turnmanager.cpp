#include "turnmanager.h"
#include <QString>

TurnManager::TurnManager(QObject* parent)
    : QObject(parent)
    , m_side(ShogiGameController::NoPlayer)
{}

TurnManager::Side TurnManager::side() const {
    return m_side;
}

void TurnManager::set(Side s) {
    const Side norm =
        (s == ShogiGameController::Player2) ? ShogiGameController::Player2 :
            (s == ShogiGameController::Player1) ? ShogiGameController::Player1 :
            ShogiGameController::Player1;

    if (m_side == norm) return;
    m_side = norm;
    emit changed(m_side);
}

void TurnManager::toggle() {
    set(m_side == ShogiGameController::Player1
            ? ShogiGameController::Player2
            : ShogiGameController::Player1);
}

QString TurnManager::toSfenToken() const {
    return (m_side == ShogiGameController::Player2)
    ? QStringLiteral("w")
    : QStringLiteral("b");
}

void TurnManager::setFromSfenToken(const QString& bw) {
    const auto s = (bw.compare(QStringLiteral("w"), Qt::CaseInsensitive) == 0)
    ? ShogiGameController::Player2
    : ShogiGameController::Player1;
    set(s);
}

int TurnManager::toClockPlayer() const {
    return (m_side == ShogiGameController::Player2) ? 2 : 1;
}

void TurnManager::setFromClockPlayer(int p) {
    set(p == 2 ? ShogiGameController::Player2
               : ShogiGameController::Player1);
}

ShogiGameController::Player TurnManager::toGc() const {
    return m_side;
}

void TurnManager::setFromGc(ShogiGameController::Player p) {
    set(p);
}
