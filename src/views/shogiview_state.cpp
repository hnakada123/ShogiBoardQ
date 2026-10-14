/// @file shogiview_state.cpp
/// @brief ShogiView の状態切替・委譲メソッド実装（対局者カードは BoardChromeWidget へ転送）

#include "shogiview.h"
#include "shogiviewhighlighting.h"
#include "shogiboard.h"
#include "boardappearance.h"

void ShogiView::refreshBoardVisuals()
{
    m_boardVisuals = BoardAppearance::instance().visuals();
    update();
}

void ShogiView::refreshBoardColors()
{
    // 対局者カードの配色は BoardChromeWidget が自分で配色の変更を受け取る
    m_boardColors = BoardAppearance::instance().colors();
    update();
}

void ShogiView::setArrows(const QList<Arrow>& arrows)
{
    m_highlighting->setArrows(arrows);
}

const QList<ShogiView::Arrow>& ShogiView::arrows() const
{
    return m_highlighting->arrows();
}

void ShogiView::clearArrows()
{
    m_highlighting->clearArrows();
}

// ─────────────────────────────────────────────────────────────────────────────
// 対局者カードの配置
// ─────────────────────────────────────────────────────────────────────────────

void ShogiView::relayoutChrome()
{
    m_chrome->setGeometry(rect());
    BoardChromeWidget::BoardLayout layout;
    layout.fieldSize = fieldSize().isValid()
        ? fieldSize()
        : QSize(m_layout.squareSize(), qRound(m_layout.squareSize() * ShogiViewLayout::kSquareAspectRatio));
    layout.nameFontScale = m_layout.nameFontScale();
    layout.blackStand = blackStandBoundingRect();
    layout.whiteStand = whiteStandBoundingRect();
    if (m_board) {
        layout.boardRect = QRect(m_layout.offsetX(), m_layout.offsetY(),
                                 layout.fieldSize.width() * m_board->files(),
                                 layout.fieldSize.height() * m_board->ranks());
    }
    layout.flipped = m_layout.flipMode();
    m_chrome->setBoardLayout(layout);
    // 駒箱はカードの下に置くので、カードを置いてから切り替えボタンを置く
    relayoutPieceBoxSideSelector();
}

void ShogiView::relayoutEditExitButton()
{
    relayoutChrome();
    m_chrome->relayoutEditExitButton();
}

void ShogiView::setNameFontScale(double scale)
{
    m_layout.setNameFontScale(scale);
    relayoutChrome();
}

void ShogiView::updateTurnIndicator(Turn now)
{
    relayoutChrome();
    m_chrome->updateTurnIndicator(now);
}

// ─────────────────────────────────────────────────────────────────────────────
// 名前・時計・手番（BoardChromeWidget へ転送。新しいコードは chrome() を直接使う）
// ─────────────────────────────────────────────────────────────────────────────

void ShogiView::setBlackClockText(const QString& text) { m_chrome->setBlackClockText(text); }
void ShogiView::setWhiteClockText(const QString& text) { m_chrome->setWhiteClockText(text); }
QLabel* ShogiView::blackClockLabel() const { return m_chrome->blackClockLabel(); }
QLabel* ShogiView::whiteClockLabel() const { return m_chrome->whiteClockLabel(); }
void ShogiView::setClockEnabled(bool enabled) { m_chrome->setClockEnabled(enabled); }
void ShogiView::setBlackPlayerName(const QString& name) { m_chrome->setBlackPlayerName(name); }
void ShogiView::setWhitePlayerName(const QString& name) { m_chrome->setWhitePlayerName(name); }
ElideLabel* ShogiView::blackNameLabel() const { return m_chrome->blackNameLabel(); }
ElideLabel* ShogiView::whiteNameLabel() const { return m_chrome->whiteNameLabel(); }
void ShogiView::setActiveSide(bool blackTurn) { m_chrome->setActiveSide(blackTurn); }
void ShogiView::setHighlightStyle(const QColor& bgOn, const QColor& fgOn, const QColor& fgOff)
{
    m_chrome->setHighlightStyle(bgOn, fgOn, fgOff);
}
void ShogiView::clearTurnHighlight() { m_chrome->clearTurnHighlight(); }
void ShogiView::setActiveIsBlack(bool activeIsBlack) { m_chrome->setActiveIsBlack(activeIsBlack); }
void ShogiView::setGameOverStyleLock(bool locked) { m_chrome->setGameOverStyleLock(locked); }
bool ShogiView::gameOverStyleLock() const { return m_chrome->gameOverStyleLock(); }
void ShogiView::setUrgencyVisuals(Urgency u) { m_chrome->setUrgencyVisuals(u); }
void ShogiView::applyClockUrgency(qint64 activeRemainMs) { m_chrome->applyClockUrgency(activeRemainMs); }
