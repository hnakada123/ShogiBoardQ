/// @file incrementaltableview.cpp
/// @brief 行の追加で表示領域全体を描き直さない表の実装

#include "incrementaltableview.h"
#include "opaquebackground.h"
#include "tableheaderview.h"

#include <QAbstractItemDelegate>
#include <QEvent>
#include <QFocusEvent>
#include <QItemSelectionModel>
#include <QHeaderView>
#include <QPaintEvent>
#include <QScrollBar>
#include <QStyle>
#include <QStyleOptionSlider>
#include <cmath>
#include <optional>
#include <utility>

namespace {

/**
 * @brief 見た目が変わらなければ描き直さないスクロールバー
 *
 * QTableView は行数が変わるたびに、スクロールバーの範囲と刻み（1ページの行数）を入れ直す。QAbstractSlider は
 * そのたびに、見た目が変わるかを見ずに描き直しを頼む（sliderChange）。行が表示領域に収まっている間は、
 * 行が増えるたびに刻みだけが変わり、つまみは全体のままで見た目は変わらない。スクロールバーは不透明で
 * ないので、描き直すたびに下にある親の部品（表・分割・パネル）も同じ範囲を描く。
 *
 * 描いたときの見た目（つまみの位置と大きさ・範囲が空か・端にあるか・大きさ・有効か）を覚え、同じなら
 * 描き直しを頼まない。マウスが乗った・押したときの描き直しは、QScrollBar が別に頼むので変わらない。
 */
class SteadyScrollBar : public QScrollBar
{
public:
    using QScrollBar::QScrollBar;

protected:
    void sliderChange(SliderChange change) override
    {
        if (m_painted && *m_painted == look()) return;
        QScrollBar::sliderChange(change);
    }

    void paintEvent(QPaintEvent* event) override
    {
        // 一部だけを描くときは、描かない所に前の見た目が残る。見た目が違えば、覚えを捨てる
        const Look now = look();
        const bool whole = (QRegion(rect()) - event->region()).isEmpty();
        if (whole) m_painted = now;
        else if (m_painted && !(*m_painted == now)) m_painted.reset();
        QScrollBar::paintEvent(event);
    }

private:
    struct Look {
        QRect slider;        ///< つまみの位置と大きさ
        bool empty = false;  ///< 範囲が空（動かせない）
        bool atMinimum = false;
        bool atMaximum = false;
        QSize size;
        bool enabled = false;

        bool operator==(const Look& other) const
        {
            return slider == other.slider && empty == other.empty && atMinimum == other.atMinimum
                && atMaximum == other.atMaximum && size == other.size && enabled == other.enabled;
        }
    };

    Look look() const
    {
        QStyleOptionSlider option;
        initStyleOption(&option);
        Look result;
        result.slider = style()->subControlRect(QStyle::CC_ScrollBar, &option, QStyle::SC_ScrollBarSlider, this);
        result.empty = minimum() == maximum();
        result.atMinimum = sliderPosition() == minimum();
        result.atMaximum = sliderPosition() == maximum();
        result.size = size();
        result.enabled = isEnabled();
        return result;
    }

    std::optional<Look> m_painted;  ///< 全体を描いたときの見た目（分からなければ nullopt）
};

} // namespace

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

    // 縦のスクロールバーを、見た目が変わらなければ描き直さないものに替える（行が増えるたびの刻みの変化で
    // 描き直さない）。QAbstractItemView は、作ったときのスクロールバーにだけ自分の処理（一番下での fetchMore・
    // スクロールの後のマウスの位置の確かめ）をつなぐので、替えたものにつなぎ直す
    auto* bar = new SteadyScrollBar(Qt::Vertical, this);
    setVerticalScrollBar(bar);
    connect(bar, &QScrollBar::actionTriggered, this, &IncrementalTableView::verticalScrollbarAction);
    connect(bar, &QScrollBar::valueChanged, this, &IncrementalTableView::verticalScrollbarValueChanged);
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
        connect(newModel, &QAbstractItemModel::modelReset, this, &IncrementalTableView::onModelReset),
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
        // 空の表に足し始めた行だけなら、どこに足しても表の行はすべて新しい行（思考表は、消した後の
        // 最初の思考情報が続けて届くと、先頭へ2行以上足す）
        if (first == rows || (m_firstRow == 0 && m_insertedRows == rows)) m_insertedRows += count;
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
    case Change::RemoveAll:
        m_change = Change::Other;
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
    // すべての行を消す（思考表の消去）なら、行を描いていた範囲だけを描き直す。行の位置は消す前に読む
    if (m_change == Change::None && parent == rootIndex() && model() && isVisible() && first == 0
        && last == model()->rowCount(rootIndex()) - 1) {
        m_changedRows.clear();
        m_change = Change::RemoveAll;
        const int top = rowViewportPosition(first);
        const int bottom = rowViewportPosition(last) + rowHeight(last);
        m_removedRowsRect = QRect(0, top, viewport()->width(), bottom - top) & viewport()->rect();
        return;
    }
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
    case Change::RemoveAll:
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

// モデルのリセットで、Qt はその場で表示領域全体の描き直しを頼み、後から配置をやり直してもう一度頼む。
// 間に描画が入って、同じ内容を2回描く。配置をここでやり直して、1回にまとめる
// （この知らせは、Qt の表と見出しがリセットを受けた後に届く）
void IncrementalTableView::onModelReset()
{
    if (isVisible()) executeDelayedItemsLayout();
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
    // 行の色を交互に変える表は、先頭に足すと既にある行の色が入れ替わるので、画面のずらしを使わない
    // （末尾への追加とすべての行の削除では、残る行の色は変わらない）
    const bool incremental = sameSettings && isVisible() && model() && !isRightToLeft()
        && (change == Change::Append || change == Change::RemoveAll
            || ((change == Change::Prepend || change == Change::RemoveBelow) && atTop && !alternatingRowColors()));
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

    // すべての行を消した後は、行を描いていた範囲が空になるだけ（残る行が無いので、画面のずらしは要らない）
    if (change == Change::RemoveAll) {
        if (!m_removedRowsRect.isEmpty()) viewport()->update(m_removedRowsRect);
        return;
    }

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
    const QEvent::Type type = event->type();
    if (type == QEvent::Paint) m_paintedViewportSize = viewport()->size();
    // ウィンドウが前面になる・後ろに回ると、Qt は表示領域の全体を描き直す（QAbstractItemView::viewportEvent。
    // ダイアログを開く・閉じるたびに、見えている表の全部で起きる）。前面と後ろでパレットが同じなら、
    // 見た目が変わりうるのは現在の行と選んだ行（スタイルが前面かどうかで描き分ける）だけなので、
    // その行だけを描き直す。Qt の処理は全体の描き直しを頼むだけなので、その先の処理へ渡す
    if ((type == QEvent::WindowActivate || type == QEvent::WindowDeactivate) && isVisible()
        && palette().isEqual(QPalette::Active, QPalette::Inactive)
        && viewport()->palette().isEqual(QPalette::Active, QPalette::Inactive)) {
        updateCurrentAndSelectedRows();
        return QAbstractScrollArea::viewportEvent(event);
    }
    return QTableView::viewportEvent(event);
}

// 表示領域の大きさの変化は、QAbstractScrollArea がこの関数に渡す
void IncrementalTableView::resizeEvent(QResizeEvent* event)
{
    const bool resizing = std::exchange(m_resizingViewport, true);
    QTableView::resizeEvent(event);
    m_resizingViewport = resizing;
}

void IncrementalTableView::focusInEvent(QFocusEvent* event)
{
    changeFocus(event, true);
}

void IncrementalTableView::focusOutEvent(QFocusEvent* event)
{
    changeFocus(event, false);
}

void IncrementalTableView::changeFocus(QFocusEvent* event, bool gained)
{
    QWidget* area = viewport();
    // Qt の処理が頼む表示領域の描き直し（全体）を受け付けずに処理させ、後から要る行だけを頼む。
    // 既に描き直しを止められているときは、そのまま Qt に任せる
    const bool limit = isVisible() && area->updatesEnabled();
    const int verticalOffset = verticalHeader()->offset();
    const int horizontalOffset = horizontalHeader()->offset();
    const QPersistentModelIndex previous = currentIndex();
    if (limit) area->setAttribute(Qt::WA_UpdatesDisabled, true);
    if (gained) QTableView::focusInEvent(event);
    else QTableView::focusOutEvent(event);
    if (!limit) return;
    area->setAttribute(Qt::WA_UpdatesDisabled, false);
    // 表示位置が動いたとき（ふつうは動かない）は全体を描き直す
    if (verticalOffset != verticalHeader()->offset() || horizontalOffset != horizontalHeader()->offset()) {
        area->update();
        return;
    }
    // フォーカスを受けたときに Qt が現在のセルを決めることがあるので、前の現在の行も描き直す
    updateCurrentAndSelectedRows(previous);
}

void IncrementalTableView::updateCurrentAndSelectedRows(const QModelIndex& alsoRow)
{
    QWidget* area = viewport();
    for (const QModelIndex& index : {alsoRow, currentIndex()}) {
        if (!index.isValid()) continue;
        area->update(0, rowViewportPosition(index.row()), area->width(), rowHeight(index.row()));
    }
    if (const QItemSelectionModel* selection = selectionModel())
        area->update(visualRegionForSelection(selection->selection()));
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
    const QBrush background = area->palette().brush(area->backgroundRole());
    const bool fill = integral && background.isOpaque();
    if (fill) {
        area->setAutoFillBackground(true);
        m_viewportFilled = true;
    } else if (m_viewportFilled) {
        area->setAutoFillBackground(false);
        m_viewportFilled = false;
    }
    // スクロールバーの入れ物も同じ条件で塗る。スクロールバーも入れ物も不透明でないので、スクロールバーを
    // 描き直すたびに、表と、その下にある親（分割・パネル）まで同じ範囲を描いていた。下に見えているのは、
    // スタイルシートが表の全体に塗る背景色（表示領域と同じ色）
    OpaqueBackground::fillScrollBarContainers(this, fill ? std::optional<QBrush>(background) : std::nullopt);
}
