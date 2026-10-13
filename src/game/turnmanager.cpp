/// @file turnmanager.cpp
/// @brief 手番の管理と各種表現形式への変換の実装

#include "turnmanager.h"
#include <QString>

// ============================================================
// 生成・基本操作
// ============================================================

TurnManager::TurnManager(QObject* parent)
    : QObject(parent)
    , m_side(Turn::Black)
{}

TurnManager::Side TurnManager::side() const {
    return m_side;
}

void TurnManager::set(Side s) {
    // Player2 以外は Player1 として正規化
    const Side norm = (s == Turn::White)
                          ? Turn::White
                          : Turn::Black;

    // 最初の設定は、既定の先手と同じでも知らせる（手番の表示を初めて合わせるため）
    if (m_sideSet && m_side == norm) return;
    m_sideSet = true;
    m_side = norm;
    emit changed(m_side);
}

void TurnManager::toggle() {
    set(m_side == Turn::Black
            ? Turn::White
            : Turn::Black);
}

// ============================================================
// SFEN "b"/"w" 変換
// ============================================================

QString TurnManager::toSfenToken() const {
    return (m_side == Turn::White)
    ? QStringLiteral("w")
    : QStringLiteral("b");
}

void TurnManager::setFromSfenToken(const QString& bw) {
    const auto s = (bw.compare(QStringLiteral("w"), Qt::CaseInsensitive) == 0)
    ? Turn::White
    : Turn::Black;
    set(s);
}

// ============================================================
// Clock 1/2 変換
// ============================================================

int TurnManager::toClockPlayer() const {
    return (m_side == Turn::White) ? 2 : 1;
}
