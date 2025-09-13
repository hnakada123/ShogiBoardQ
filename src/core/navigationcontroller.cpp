#include "navigationcontroller.h"
#include "navigationcontext.h"
#include <QPushButton>
#include <QtGlobal>

NavigationController::NavigationController(const Buttons& b,
                                           INavigationContext* ctx,
                                           QObject* parent)
    : QObject(parent), m_ctx(ctx)
{
    if (b.first)  connect(b.first,  &QPushButton::clicked, this, [this](bool){ toFirst();  });
    if (b.back10) connect(b.back10, &QPushButton::clicked, this, [this](bool){ back10();   });
    if (b.prev)   connect(b.prev,   &QPushButton::clicked, this, [this](bool){ prev();     });
    if (b.next)   connect(b.next,   &QPushButton::clicked, this, [this](bool){ next();     });
    if (b.fwd10)  connect(b.fwd10,  &QPushButton::clicked, this, [this](bool){ fwd10();    });
    if (b.last)   connect(b.last,   &QPushButton::clicked, this, [this](bool){ toLast();   });
}

int NavigationController::clampRow(int row) const {
    const int n = m_ctx->resolvedRowCount();
    return qBound(0, row, n > 0 ? n - 1 : 0);
}

void NavigationController::toFirst() {
    if (!m_ctx->hasResolvedRows()) return;
    const int row = clampRow(m_ctx->activeResolvedRow());
    m_ctx->applySelect(row, 0);
}

void NavigationController::back10() {
    if (!m_ctx->hasResolvedRows()) return;
    const int row    = clampRow(m_ctx->activeResolvedRow());
    const int cur    = qBound(0, m_ctx->currentPly(), m_ctx->maxPlyAtRow(row));
    const int target = qMax(0, cur - 10);
    m_ctx->applySelect(row, target);
}

void NavigationController::prev() {
    if (!m_ctx->hasResolvedRows()) return;
    const int row    = clampRow(m_ctx->activeResolvedRow());
    const int cur    = qBound(0, m_ctx->currentPly(), m_ctx->maxPlyAtRow(row));
    const int target = qMax(0, cur - 1);
    m_ctx->applySelect(row, target);
}

void NavigationController::next() {
    if (!m_ctx->hasResolvedRows()) return;
    const int row     = clampRow(m_ctx->activeResolvedRow());
    const int maxPly  = m_ctx->maxPlyAtRow(row);
    const int cur     = qBound(0, m_ctx->currentPly(), maxPly);
    if (cur >= maxPly) return;
    m_ctx->applySelect(row, cur + 1);
}

void NavigationController::fwd10() {
    if (!m_ctx->hasResolvedRows()) return;
    const int row     = clampRow(m_ctx->activeResolvedRow());
    const int maxPly  = m_ctx->maxPlyAtRow(row);
    const int cur     = qBound(0, m_ctx->currentPly(), maxPly);
    if (cur >= maxPly) return;
    const int target  = qMin(maxPly, cur + 10);
    m_ctx->applySelect(row, target);
}

void NavigationController::toLast() {
    if (!m_ctx->hasResolvedRows()) return;
    const int row     = clampRow(m_ctx->activeResolvedRow());
    const int lastPly = m_ctx->maxPlyAtRow(row);
    m_ctx->applySelect(row, lastPly);
}
