/// @file turnmanager.cpp
/// @brief 手番の管理と各種表現形式への変換の実装

#include "turnmanager.h"
#include <QString>

// ============================================================
// 生成・基本操作
// ============================================================

TurnManager::TurnManager(QObject* parent)
    : QObject(parent)
    , m_side(ShogiGameController::NoPlayer)
{}

TurnManager::Side TurnManager::side() const {
    return m_side;
}

void TurnManager::set(Side s) {
    // Player2 以外は Player1 として正規化
    const Side norm = (s == ShogiGameController::Player2)
                          ? ShogiGameController::Player2
                          : ShogiGameController::Player1;

    if (m_side == norm) return;
    m_side = norm;
    emit changed(m_side);
}

void TurnManager::toggle() {
    set(m_side == ShogiGameController::Player1
            ? ShogiGameController::Player2
            : ShogiGameController::Player1);
}

// ============================================================
// Turn enum 変換
// ============================================================

Turn TurnManager::toTurn() const {
    return (m_side == ShogiGameController::Player2)
               ? Turn::White : Turn::Black;
}

void TurnManager::setFromTurn(Turn t) {
    set(t == Turn::White
            ? ShogiGameController::Player2
            : ShogiGameController::Player1);
}

// ============================================================
// SFEN "b"/"w" 変換
// ============================================================

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

// ============================================================
// Clock 1/2 変換
// ============================================================

int TurnManager::toClockPlayer() const {
    return (m_side == ShogiGameController::Player2) ? 2 : 1;
}

void TurnManager::setFromClockPlayer(int p) {
    set(p == 2 ? ShogiGameController::Player2
               : ShogiGameController::Player1);
}

// ============================================================
// GameController 変換
// ============================================================

ShogiGameController::Player TurnManager::toGc() const {
    return m_side;
}

void TurnManager::setFromGc(ShogiGameController::Player p) {
    set(p);
}
