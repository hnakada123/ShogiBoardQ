/// @file josekimoveboard.cpp
/// @brief 定跡手ダイアログの盤の実装

#include "josekimoveboard.h"

#include "boardinteractioncontroller.h"
#include "boardviewerwidget.h"
#include "josekisettings.h"
#include "promotedialog.h"
#include "shogiboard.h"
#include "shogiview.h"
#include "usimove.h"

#include <QVBoxLayout>

#include <functional>
#include <optional>

JosekiMoveBoard::JosekiMoveBoard(QWidget* parent)
    : QWidget(parent)
    , m_viewer(new BoardViewerWidget(BoardViewerWidget::ZoomToolbar | BoardViewerWidget::FitWindowOnZoom,
                                     BoardViewerWidget::ButtonLabels::Symbols, nullptr, this))
    // 成り・不成の両方が指せる手は、主画面と同じダイアログで尋ねる
    , m_interaction(new BoardInteractionController(m_viewer->view(), m_viewer->board(),
                                                   std::bind(&PromoteDialog::ask, this), this))
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_viewer);

    ShogiView* boardView = m_viewer->view();
    boardView->setObjectName(QStringLiteral("josekiMoveBoardView"));
    boardView->setSquareSize(JosekiSettings::josekiMoveBoardSquareSize());
    m_viewer->setFlipped(JosekiSettings::josekiMoveBoardFlipped());

    connect(boardView, &ShogiView::clicked, m_interaction, &BoardInteractionController::onLeftClick);
    connect(boardView, &ShogiView::rightClicked, m_interaction, &BoardInteractionController::onRightClick);
    connect(boardView, &ShogiView::fieldSizeChanged, this, &JosekiMoveBoard::saveSquareSize);
    connect(m_viewer, &BoardViewerWidget::boardTransformed, this, &JosekiMoveBoard::cancelSelection);
    connect(m_viewer, &BoardViewerWidget::flipToggled, this, &JosekiMoveBoard::saveFlipped);
    m_interaction->setInputRoute(this);
}

ShogiView* JosekiMoveBoard::view() const
{
    return m_viewer->view();
}

void JosekiMoveBoard::showPosition(const QString& sfen, const QString& lastMove, bool acceptsInput)
{
    m_acceptsInput = acceptsInput && !sfen.isEmpty();
    if (!m_acceptsInput) m_interaction->cancelPendingClick();
    if (sfen.isEmpty() || (sfen == m_sfen && lastMove == m_lastMove)) return;

    m_sfen = sfen;
    m_lastMove = lastMove;
    m_viewer->board()->setSfen(sfen);
    m_interaction->showLastMove(lastMove);
}

void JosekiMoveBoard::submit(const QPoint& from, const QPoint& to)
{
    // ドラッグの表示は盤の操作が終える（成りを尋ねている間は保つ）
    const std::optional<UsiMove> move = m_interaction->moveFromClick(from, to);
    if (move) emit movePlayed(move->toString());
    m_interaction->onMoveApplied(from, to, move.has_value());
}

void JosekiMoveBoard::cancelSelection()
{
    m_interaction->cancelInput();
}

void JosekiMoveBoard::saveSquareSize()
{
    JosekiSettings::josekiMoveBoardSquareSize.set(m_viewer->view()->squareSize());
}

void JosekiMoveBoard::saveFlipped(bool flipped)
{
    JosekiSettings::josekiMoveBoardFlipped.set(flipped);
}
