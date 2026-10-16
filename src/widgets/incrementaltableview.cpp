/// @file incrementaltableview.cpp
/// @brief 行の追加で表示領域全体を描き直さない表の実装

#include "incrementaltableview.h"
#include "tableheaderview.h"

#include <QAbstractItemDelegate>
#include <QEvent>
#include <QHeaderView>
#include <QScrollBar>
#include <cmath>
#include <utility>

bool IncrementalTableView::Settings::operator==(const Settings& other) const
{
    return font == other.font && iconSize == other.iconSize && rootIndex == other.rootIndex
        && style == other.style && delegate == other.delegate && columnDelegates == other.columnDelegates;
}

IncrementalTableView::IncrementalTableView(QWidget* parent)
    : QTableView(parent)
{
    // 見出しの文字の測り直しと、見た目の変わらない描き直しを省く見出しにする（QTableView の既定と同じ設定）
    auto* header = TableHeaderView::install(this);
    // 列幅の変化は、Qt の処理（後から配置を直して表示領域を描き直す）の前に、
    // 表示領域の大きさの変化によるものかを見分ける
    disconnect(header, &QHeaderView::sectionResized, this, &IncrementalTableView::columnResized);
    connect(header, &QHeaderView::sectionResized, this, &IncrementalTableView::onColumnResized);
}

IncrementalTableView::Settings IncrementalTableView::currentSettings() const
{
    Settings settings;
    settings.font = font();
    settings.iconSize = iconSize();
    settings.rootIndex = rootIndex();
    settings.style = style();
    settings.delegate = itemDelegate();
    const int columns = model() ? model()->columnCount(rootIndex()) : 0;
    settings.columnDelegates.reserve(columns);
    for (int column = 0; column < columns; ++column)
        settings.columnDelegates.append(itemDelegateForColumn(column));
    return settings;
}

void IncrementalTableView::setModel(QAbstractItemModel* newModel)
{
    if (newModel == model()) {
        QTableView::setModel(newModel);
        return;
    }
    for (const QMetaObject::Connection& connection : std::as_const(m_modelConnections)) disconnect(connection);
    m_modelConnections.clear();
    QTableView::setModel(newModel);
    m_change = Change::Other;
    m_changedRows.clear();
    if (!newModel) return;

    // 追加の種類は、Qt やほかの部品が追加後の通知で配置をやり直す前に知る必要がある。
    // 追加前の通知（rowsAboutToBeInserted）は、それらより必ず先に届く。
    m_modelConnections = {
        connect(newModel, &QAbstractItemModel::rowsAboutToBeInserted,
                this, &IncrementalTableView::onRowsAboutToBeInserted),
        connect(newModel, &QAbstractItemModel::dataChanged, this, &IncrementalTableView::onDataChanged),
        connect(newModel, &QAbstractItemModel::rowsAboutToBeRemoved, this, &IncrementalTableView::onRowsAboutToBeRemoved),
        connect(newModel, &QAbstractItemModel::rowsAboutToBeMoved, this, &IncrementalTableView::onStructureChanged),
        connect(newModel, &QAbstractItemModel::columnsAboutToBeInserted,
                this, &IncrementalTableView::onStructureChanged),
        connect(newModel, &QAbstractItemModel::columnsAboutToBeRemoved,
                this, &IncrementalTableView::onStructureChanged),
        connect(newModel, &QAbstractItemModel::columnsAboutToBeMoved, this, &IncrementalTableView::onStructureChanged),
        connect(newModel, &QAbstractItemModel::modelAboutToBeReset, this, &IncrementalTableView::onStructureChanged),
        connect(newModel, &QAbstractItemModel::layoutAboutToBeChanged, this, &IncrementalTableView::onStructureChanged),
    };
}

void IncrementalTableView::onRowsAboutToBeInserted(const QModelIndex& parent, int first, int last)
{
    if (parent != rootIndex() || !model()) {
        m_change = Change::Other;
        return;
    }
    const int count = last - first + 1;
    const int rows = model()->rowCount(rootIndex());  // 追加前の行数
    switch (m_change) {
    case Change::None:
        m_changedRows.clear();
        m_firstRow = first;
        m_insertedRows = count;
        m_change = first == rows ? Change::Append : first == 0 ? Change::Prepend : Change::Other;
        break;
    case Change::Append:
        if (first == rows) m_insertedRows += count;
        else m_change = Change::Other;
        break;
    case Change::Prepend:
    case Change::RemoveBelow:
        if (first == 0) {
            if (m_change == Change::RemoveBelow) {
                m_firstRow = 0;
                m_insertedRows = 0;
                m_change = Change::Prepend;
            }
            m_insertedRows += count;
            // 先に内容が変わった行も、この追加の分だけ下にずれる
            for (auto& range : m_changedRows) {
                range.first += count;
                range.second += count;
            }
        } else {
            m_change = Change::Other;
        }
        break;
    case Change::Other:
        break;
    }
}

// 思考表は行数の上限を超えると、先頭に足すたびに末尾の行を消す。一番上を表示していて、
// 消す行が表示領域より下なら、見えている行の位置も内容も変わらないので部分描画を続ける。
// 行の位置は、見出しが追加の通知をその場で反映しているので、追加後の値で比べられる。
void IncrementalTableView::onRowsAboutToBeRemoved(const QModelIndex& parent, int first, int last)
{
    Q_UNUSED(last)
    const bool belowView = parent == rootIndex() && model() && isVisible()
        && verticalScrollBar()->value() == 0 && rowViewportPosition(first) >= viewport()->height();
    if (!belowView) {
        m_change = Change::Other;
        return;
    }
    switch (m_change) {
    case Change::None:
        m_changedRows.clear();
        m_change = Change::RemoveBelow;
        break;
    case Change::Prepend:
        // 足した行の一部も消えるなら、残った分だけを描く
        if (first < m_firstRow + m_insertedRows) m_insertedRows = first - m_firstRow;
        if (m_insertedRows <= 0) m_change = Change::Other;
        break;
    case Change::RemoveBelow:
        break;
    case Change::Append:
    case Change::Other:
        m_change = Change::Other;
        return;
    }
    // 内容が変わった行のうち消える分は、表示領域より下なので描かなくてよい
    for (qsizetype i = m_changedRows.size() - 1; i >= 0; --i) {
        auto& range = m_changedRows[i];
        if (range.first >= first) m_changedRows.removeAt(i);
        else if (range.second >= first) range.second = first - 1;
    }
}

// 配置のやり直しを待つ間、Qt は内容が変わった行を描き直さない（全体を描き直す前提のため）。
// 全体を描き直さない場合に備えて、その行を覚えておく。
void IncrementalTableView::onDataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight)
{
    if (m_change != Change::Append && m_change != Change::Prepend && m_change != Change::RemoveBelow) return;
    if (!topLeft.isValid() || !bottomRight.isValid() || topLeft.parent() != rootIndex()) {
        m_change = Change::Other;
        return;
    }
    m_changedRows.append({topLeft.row(), bottomRight.row()});
}

void IncrementalTableView::onStructureChanged()
{
    m_change = Change::Other;
}

void IncrementalTableView::doItemsLayout()
{
    const Change change = std::exchange(m_change, Change::None);
    const QList<QPair<int, int>> changedRows = std::exchange(m_changedRows, {});
    const Settings settings = currentSettings();
    const bool sameSettings = settings == m_lastLayoutSettings;
    m_lastLayoutSettings = settings;

    // 先頭への追加で画面をずらすのは、一番上を表示しているときだけ
    const bool atTop = verticalScrollBar()->value() == 0;
    const bool incremental = sameSettings && isVisible() && model() && !alternatingRowColors()
        && !isRightToLeft()
        && (change == Change::Append || ((change == Change::Prepend || change == Change::RemoveBelow) && atTop));
    if (!incremental) {
        QTableView::doItemsLayout();
        return;
    }

    // QAbstractItemView::doItemsLayout と同じく行・列の配置と見出し・スクロールバーを更新し、
    // 表示領域全体の描き直しの代わりに、変わった所だけを描き直す
    const int verticalOffset = verticalHeader()->offset();
    const int horizontalOffset = horizontalHeader()->offset();
    updateGeometries();
    if (!verticalHeader()->updatesEnabled()) verticalHeader()->setUpdatesEnabled(true);

    // 行が増えると、スクロールバーの値が同じでも表示位置が変わることがある
    // （一番下まで表示していると、最後の行を下端に合わせていたのを行の上端に合わせ直す）。
    // その分と、先頭に追加した行の高さの分だけ、既にある行を画面のずらしで動かす
    // （表示領域が不透明でなければ、Qt が全体を描き直す）。
    const int shift = change == Change::Prepend
        ? rowViewportPosition(m_firstRow + m_insertedRows) - rowViewportPosition(m_firstRow) : 0;
    const int dx = horizontalOffset - horizontalHeader()->offset();
    const int dy = verticalOffset - verticalHeader()->offset() + shift;
    QWidget* area = viewport();
    if (dx != 0 || dy != 0) {
        area->scroll(dx, dy);
        // 見出しを隠して罫線を引く表は、元の先頭の行の上の罫線も引き直す（QTableView::scrollContentsBy と同じ）
        if (dy > 0 && showGrid() && horizontalHeader()->isHidden()) area->update(0, dy, area->width(), dy);
    }

    // 追加した行と、その間に内容が変わった行を描き直す。棋譜欄は追加の直後に一番下まで
    // スクロールする。描き直しの予約が残ったまま画面をずらすと、ずらしたはずの所が
    // 古いまま残ることがあるので、行の位置が決まってから予約する。
    QAbstractItemModel* items = model();
    if (change != Change::RemoveBelow) {
        m_pendingRows.append(items->index(m_firstRow, 0, rootIndex()));
        m_pendingRows.append(items->index(m_firstRow + m_insertedRows - 1, 0, rootIndex()));
    }
    for (const auto& [top, bottom] : changedRows) {
        m_pendingRows.append(items->index(top, 0, rootIndex()));
        m_pendingRows.append(items->index(bottom, 0, rootIndex()));
    }
    if (!m_flushQueued && !m_pendingRows.isEmpty()) {
        m_flushQueued = true;
        QMetaObject::invokeMethod(this, &IncrementalTableView::flushRowUpdates, Qt::QueuedConnection);
    }
}

void IncrementalTableView::flushRowUpdates()
{
    m_flushQueued = false;
    const QList<QPersistentModelIndex> rows = std::exchange(m_pendingRows, {});
    QWidget* area = viewport();
    for (qsizetype i = 0; i + 1 < rows.size(); i += 2) {
        if (!rows.at(i).isValid() || !rows.at(i + 1).isValid()) {
            area->update();  // 行が消えていれば全体を描き直す
            return;
        }
        const int top = rowViewportPosition(rows.at(i).row());
        const int bottom = rowViewportPosition(rows.at(i + 1).row()) + rowHeight(rows.at(i + 1).row());
        if (bottom > 0 && top < area->height()) area->update(0, top, area->width(), bottom - top);
    }
}

// 縦のスクロールバーが出入りすると、表示領域の幅が変わって最後の列が伸び縮みする。
// Qt の列幅の処理は、後から配置を直して表示領域全体の描き直しを頼む。表示領域の大きさの
// 変化を処理している間（見出しの幅の更新で最後の列が伸び縮みする）は、配置はその続きで
// 直る。前回描いてから表示領域の大きさが変わっていれば、全体の描き直しも予約済みなので、
// Qt の処理を省く（描き直した後で、同じ内容をもう一度描くことになる）。
void IncrementalTableView::onColumnResized(int column, int oldWidth, int newWidth)
{
    if (m_resizingViewport && viewport()->size() != m_paintedViewportSize) return;
    QTableView::columnResized(column, oldWidth, newWidth);
}

bool IncrementalTableView::viewportEvent(QEvent* event)
{
    if (event->type() == QEvent::Paint) m_paintedViewportSize = viewport()->size();
    return QTableView::viewportEvent(event);
}

// 表示領域の大きさの変化は、QAbstractScrollArea がこの関数に渡す
void IncrementalTableView::resizeEvent(QResizeEvent* event)
{
    const bool resizing = std::exchange(m_resizingViewport, true);
    QTableView::resizeEvent(event);
    m_resizingViewport = resizing;
}

bool IncrementalTableView::event(QEvent* event)
{
    const QEvent::Type type = event->type();
    // スタイルシートの付け直しは見た目全体に関わるので、全体を描き直す
    if (type == QEvent::StyleChange) m_change = Change::Other;
    const bool result = QTableView::event(event);
    switch (type) {
    case QEvent::Polish:
    case QEvent::StyleChange:
    case QEvent::PaletteChange:
    case QEvent::DevicePixelRatioChange:
    case QEvent::Show:
        updateViewportFill();
        break;
    default:
        break;
    }
    return result;
}

void IncrementalTableView::updateViewportFill()
{
    // スタイルシートで背景を描く表示領域（QStyleSheetStyle が自動の塗りつぶしを止めたもの）だけを扱う
    QWidget* area = viewport();
    if (!area->testAttribute(Qt::WA_StyledBackground)) return;
    const qreal ratio = devicePixelRatioF();
    const bool integral = std::abs(ratio - std::round(ratio)) < 1e-9;
    if (integral && area->palette().brush(area->backgroundRole()).isOpaque()) {
        area->setAutoFillBackground(true);
        m_viewportFilled = true;
    } else if (m_viewportFilled) {
        area->setAutoFillBackground(false);
        m_viewportFilled = false;
    }
}
