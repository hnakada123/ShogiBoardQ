#include "navigationcontroller.h"
#include "navigationcontext.h"
#include <QPushButton>
#include <QtGlobal>

NavigationController::NavigationController(const Buttons& b,
                                           INavigationContext* ctx,
                                           QObject* parent)
    : QObject(parent), m_ctx(ctx)
{
    if (b.first)  connect(b.first,  &QPushButton::clicked, this, &NavigationController::toFirst);
    if (b.back10) connect(b.back10, &QPushButton::clicked, this, &NavigationController::back10);
    if (b.prev)   connect(b.prev,   &QPushButton::clicked, this, &NavigationController::prev);
    if (b.next)   connect(b.next,   &QPushButton::clicked, this, &NavigationController::next);
    if (b.fwd10)  connect(b.fwd10,  &QPushButton::clicked, this, &NavigationController::fwd10);
    if (b.last)   connect(b.last,   &QPushButton::clicked, this, &NavigationController::toLast);
}

int NavigationController::clampRow(int row) const {
    const int n = m_ctx->resolvedRowCount();
    return qBound(0, row, n > 0 ? n - 1 : 0);
}

void NavigationController::toFirst(bool /*checked*/) {
    const bool has = m_ctx->hasResolvedRows();
    const int  row = has ? clampRow(m_ctx->activeResolvedRow()) : 0;
    m_ctx->applySelect(row, 0);
}

void NavigationController::back10(bool /*checked*/) {
    const bool has = m_ctx->hasResolvedRows();
    const int  row = has ? clampRow(m_ctx->activeResolvedRow()) : 0;

    const int maxPly = m_ctx->maxPlyAtRow(row);                 // 空なら MainWindow 側でモデル行数-1から算出
    const int cur    = qBound(0, m_ctx->currentPly(), maxPly);
    const int target = qMax(0, cur - 10);

    m_ctx->applySelect(row, target);
}

void NavigationController::prev(bool /*checked*/) {
    const bool has = m_ctx->hasResolvedRows();
    const int  row = has ? clampRow(m_ctx->activeResolvedRow()) : 0;

    const int maxPly = m_ctx->maxPlyAtRow(row);
    const int cur    = qBound(0, m_ctx->currentPly(), maxPly);
    const int target = qMax(0, cur - 1);

    qDebug().noquote() << "[NavCtrl-DEBUG] prev(): hasResolvedRows=" << has
                       << "row=" << row << "maxPly=" << maxPly
                       << "currentPly=" << m_ctx->currentPly() << "cur=" << cur
                       << "target=" << target;

    m_ctx->applySelect(row, target);
}

void NavigationController::next(bool /*checked*/) {
    // 解決済み行が無くても続行する（row は 0 扱い）
    const bool has = m_ctx->hasResolvedRows();
    const int  row = has ? clampRow(m_ctx->activeResolvedRow()) : 0;

    const int maxPly = m_ctx->maxPlyAtRow(row);         // 空の時は MainWindow 側でモデル行数から算出
    const int cur    = qBound(0, m_ctx->currentPly(), maxPly);
    if (cur >= maxPly) return;

    m_ctx->applySelect(row, cur + 1);                    // 空の時は MainWindow 側のフォールバックが実行
}

void NavigationController::fwd10(bool /*checked*/) {
    const bool has = m_ctx->hasResolvedRows();
    const int  row = has ? clampRow(m_ctx->activeResolvedRow()) : 0;

    const int maxPly = m_ctx->maxPlyAtRow(row);
    const int cur    = qBound(0, m_ctx->currentPly(), maxPly);
    if (cur >= maxPly) return;                                   // 末尾なら何もしない

    const int target = qMin(maxPly, cur + 10);
    m_ctx->applySelect(row, target);
}

void NavigationController::toLast(bool /*checked*/) {
    const bool has   = m_ctx->hasResolvedRows();
    const int  row   = has ? clampRow(m_ctx->activeResolvedRow()) : 0;
    const int  lastPly = m_ctx->maxPlyAtRow(row);                // 空なら 0（開始局面）想定

    m_ctx->applySelect(row, lastPly);
}
