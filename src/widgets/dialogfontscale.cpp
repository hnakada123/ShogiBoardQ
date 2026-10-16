#include "dialogfontscale.h"
#include "fontsizehelper.h"

#include "appsettings.h"
#include "dialogutils.h"
#include "fontsizecontrol.h"

#include <QBoxLayout>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QEvent>
#include <QGridLayout>
#include <QHash>
#include <QLabel>
#include <QLatin1StringView>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QScreen>
#include <QTimer>
#include <QComboBox>
#include <QAbstractItemView>
#include <QStyle>
#include <memory>
#include <vector>

namespace {

// 幅が足りないと文字サイズ操作と決定ボタンを二段にする。
// 横並びの最小幅をウィンドウに強制しないため、縮小後も折り返せる。
class DialogFooterLayout final : public QLayout
{
public:
    DialogFooterLayout() { setContentsMargins(0, 0, 0, 0); setSpacing(10); }
    void setPreferSingleRow(bool prefer) { m_preferSingleRow = prefer; }
    void addItem(QLayoutItem* item) override { m_items.emplace_back(item); }
    int count() const override { return static_cast<int>(m_items.size()); }
    QLayoutItem* itemAt(int index) const override
    {
        return index >= 0 && index < count() ? m_items[static_cast<size_t>(index)].get() : nullptr;
    }
    QLayoutItem* takeAt(int index) override
    {
        if (index < 0 || index >= count()) return nullptr;
        auto item = std::move(m_items[static_cast<size_t>(index)]);
        m_items.erase(m_items.begin() + index);
        return item.release();
    }
    QSize minimumSize() const override
    {
        QSize size;
        for (const auto& item : m_items) size = size.expandedTo(item->minimumSize());
        // QMessageBox は最小の幅で大きさを決めるため、短い文でも一段に並ぶよう一段分の幅を最小にする。
        // 画面の半分を超える大きな文字では二段に折り返す
        if (m_preferSingleRow && count() == 2) {
            const QWidget* widget = parentWidget();
            const QScreen* screen = widget ? widget->screen() : nullptr;
            const int singleRow = sizeHint().width();
            if (screen && singleRow <= screen->availableGeometry().width() / 2)
                size.setWidth(qMax(size.width(), singleRow));
        }
        return size;
    }
    QSize sizeHint() const override
    {
        if (count() != 2) return minimumSize();
        const auto first = itemAt(0)->sizeHint();
        const auto last = itemAt(1)->sizeHint();
        return {first.width() + spacing() + last.width(), qMax(first.height(), last.height())};
    }
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int width) const override
    {
        if (count() != 2) return minimumSize().height();
        const int first = itemAt(0)->sizeHint().height();
        const int last = itemAt(1)->sizeHint().height();
        return width >= sizeHint().width() ? qMax(first, last) : first + spacing() + last;
    }
    void setGeometry(const QRect& rect) override
    {
        QLayout::setGeometry(rect);
        if (count() != 2) return;
        const auto first = itemAt(0)->sizeHint();
        const auto last = itemAt(1)->sizeHint();
        const bool fits = rect.width() >= sizeHint().width();
        const QRect fontRect(rect.x(), rect.y() + (fits ? (rect.height() - first.height()) / 2 : 0),
                             fits ? first.width() : rect.width(), first.height());
        const QRect actionsRect(fits ? rect.x() + first.width() + spacing() : rect.x(),
            fits ? rect.y() + (rect.height() - last.height()) / 2 : rect.y() + first.height() + spacing(),
            fits ? rect.width() - first.width() - spacing() : rect.width(), last.height());
        const auto direction = parentWidget()->layoutDirection();
        itemAt(0)->setGeometry(QStyle::visualRect(direction, rect, fontRect));
        itemAt(1)->setGeometry(QStyle::visualRect(direction, rect, actionsRect));
    }
private:
    std::vector<std::unique_ptr<QLayoutItem>> m_items;
    bool m_preferSingleRow = false;
};

void collectFonts(QWidget* root, QList<QPair<QWidget*, QFont>>& fonts)
{
    fonts.append({root, root->font()});
    const auto children = root->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly);
    for (auto* child : children) {
        // 別ウィンドウ（子ダイアログなど）には伝えない。ダイアログのボタンから開くメニューは同じ文字で出す
        if ((child->isWindow() && !qobject_cast<QMenu*>(child)) || child->property("dialogFontScaleExcluded").toBool())
            continue;
        collectFonts(child, fonts);
    }
}

// QMessageBox の静的関数で出すボックスには呼び出し側から触れないため、ポリッシュ時（表示の直前）に付ける
class MessageBoxFontScaleInstaller final : public QObject
{
public:
    using QObject::QObject;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (event->type() == QEvent::Polish) {
            auto* box = qobject_cast<QMessageBox*>(watched);
            if (box && !box->findChild<QWidget*>(QStringLiteral("dialogFontScale"), Qt::FindDirectChildrenOnly))
                DialogFontScale::install(box, AppSettings::messageBoxFontSize);
        }
        return QObject::eventFilter(watched, event);
    }
};
}

DialogFontScale* DialogFontScale::install(QDialog* dialog, const Setting<int>& fontSize, const Options& options)
{
    return new DialogFontScale(dialog, fontSize, options);
}

void DialogFontScale::installForMessageBoxes(QCoreApplication* app)
{
    app->installEventFilter(new MessageBoxFontScaleInstaller(app));
}

QFont DialogFontScale::messageBoxFont(const QFont& font)
{
    // install() と同じく、ボックスの文字の大きさを既定値として保存済みの大きさを読む
    QFont result = font;
    result.setPointSize(qBound(8, AppSettings::messageBoxFontSize.valueOr(qMax(8, font.pointSize())), 24));
    return result;
}

DialogFontScale::DialogFontScale(QDialog* dialog, const Setting<int>& fontSize, const Options& options)
    : QWidget(dialog), m_dialog(dialog), m_baseSize(qMax(8, dialog->font().pointSize())), m_sizeSetting(options.size),
      m_ownButtons(!(options.decrease && options.increase))
{
    // 既定値のある項目はその値、既定値が 0 の項目は未保存ならダイアログの元の大きさから始める
    const int initial = fontSize.defaultValue() > 0 ? fontSize() : fontSize.valueOr(m_baseSize);
    m_fontSize = new FontSizeHelper({initial, options.minSize, options.maxSize, 1,
                                     [&fontSize](int size) { fontSize.set(size); }},
                                    this);
    setObjectName(QStringLiteral("dialogFontScale"));
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(m_fontSize, &FontSizeHelper::fontSizeChanged, this, &DialogFontScale::applySize);
    if (m_ownButtons) {
        auto* row = new QHBoxLayout(this);
        row->setContentsMargins(0, 0, 0, 0);
        row->setSpacing(6);
        auto* decreaseButton = new QPushButton(this);
        auto* increaseButton = new QPushButton(this);
        decreaseButton->setObjectName(QStringLiteral("fontDecrease"));
        increaseButton->setObjectName(QStringLiteral("fontIncrease"));
        m_sizeLabel = new QLabel(this);
        m_sizeLabel->setObjectName(QStringLiteral("dialogFontSize"));
        m_sizeLabel->setAlignment(Qt::AlignCenter);
        m_sizeLabel->setToolTip(QCoreApplication::translate("DialogFontScale", "この画面の文字サイズ"));
        row->addWidget(decreaseButton);
        row->addWidget(m_sizeLabel);
        row->addWidget(increaseButton);
        row->addStretch();
        m_buttons = new FontSizeControl(decreaseButton, increaseButton, m_fontSize, FontSizeControl::Sizing::Dialog);
    } else {
        // ダイアログが置いた A-/A+ を使う（この部品は表示しない）
        hide();
        m_sizeLabel = options.sizeLabel;
        m_buttons = new FontSizeControl(options.decrease, options.increase, m_fontSize, FontSizeControl::Sizing::Dialog);
        m_hostLayout = dialog->layout();
        m_attached = true;
    }
    if (attachToLayout()) {
        // メッセージボックスは既定のボタンの強調（アプリ共通のスタイル）を残す
        if (!qobject_cast<QMessageBox*>(dialog)) DialogUtils::standardizeDialog(dialog);
        applySize(m_fontSize->fontSize());
    }
    if (m_sizeSetting) DialogUtils::restoreDialogSize(dialog, *m_sizeSetting);
    dialog->installEventFilter(this);
}

bool DialogFontScale::attachToLayout()
{
    if (m_attached) return true;
    if (auto* box = qobject_cast<QBoxLayout*>(m_dialog->layout())) {
        auto* last = box->count() ? box->itemAt(box->count() - 1) : nullptr;
        auto* actions = last ? qobject_cast<QDialogButtonBox*>(last->widget()) : nullptr;
        if (actions) {
            box->removeWidget(actions);
            auto* footer = new DialogFooterLayout;
            footer->addWidget(this);
            footer->addWidget(actions);
            box->addLayout(footer);
            m_footer = footer;
        } else if (last && qobject_cast<QHBoxLayout*>(last->layout())) {
            static_cast<QHBoxLayout*>(last->layout())->insertWidget(0, this);
        } else {
            box->insertWidget(qMax(0, box->count() - 1), this);
        }
    } else if (auto* grid = qobject_cast<QGridLayout*>(m_dialog->layout())) {
        // QMessageBox は決定ボタンを最下段に置くので、その位置で文字サイズ操作と並べる
        auto* actions = m_dialog->findChild<QDialogButtonBox*>(QString(), Qt::FindDirectChildrenOnly);
        const int index = actions ? grid->indexOf(actions) : -1;
        if (index >= 0) {
            int row = 0, column = 0, rowSpan = 0, columnSpan = 0;
            grid->getItemPosition(index, &row, &column, &rowSpan, &columnSpan);
            grid->removeWidget(actions);
            auto* footer = new DialogFooterLayout;
            footer->setPreferSingleRow(qobject_cast<QMessageBox*>(m_dialog) != nullptr);
            footer->addWidget(this);
            footer->addWidget(actions);
            grid->addLayout(footer, row, column, rowSpan, columnSpan);
            m_footer = footer;
        } else
            grid->addWidget(this, grid->rowCount(), 0, 1, grid->columnCount());
    } else return false;
    m_hostLayout = m_dialog->layout();
    m_attached = true;
    return true;
}

void DialogFontScale::applySize(int size)
{
    // 先に収集してから適用し、親からの伝播による二重拡大を防ぐ。
    QList<QPair<QWidget*, QFont>> fonts;
    collectFonts(m_dialog, fonts);
    QHash<const QWidget*, int> collectedSize;
    for (const auto& entry : std::as_const(fonts)) collectedSize.insert(entry.first, entry.second.pointSize());
    for (const auto& entry : std::as_const(fonts)) {
        QWidget* widget = entry.first;
        QFont font = entry.second;
        if (!widget->property("dialogFontBaseSize").isValid()) {
            // 後から作られた部品は、親の今の文字を受け継いでいれば親の元の大きさを元にする
            // （そのまま差を足すと二重に拡大する）。受け継いでいなければ自分の大きさを元にする
            const QWidget* parent = widget->parentWidget();
            const bool inherited = parent && parent->property("dialogFontBaseSize").isValid()
                && collectedSize.value(parent, -1) == font.pointSize();
            widget->setProperty("dialogFontBaseSize",
                                inherited ? parent->property("dialogFontBaseSize").toInt() : font.pointSize());
        }
        const int baseSize = widget->property("dialogFontBaseSize").toInt();
        // 見出しなどの小さい文字も 8pt（範囲の下限がそれより小さければ下限）より小さくしない
        font.setPointSize(qMax(qMin(8, m_fontSize->minSize()), baseSize + size - m_baseSize));
        widget->setFont(font);
        if (auto* combo = qobject_cast<QComboBox*>(widget)) combo->view()->setFont(font);
    }
    if (m_sizeLabel) m_sizeLabel->setText(QStringLiteral("%1 pt").arg(size));
    // A-/A+ の寸法は付け替えた文字で決め直してから、操作列の高さを測る
    m_buttons->refresh();
    if (layout()) {
        layout()->invalidate();
        setMinimumHeight(layout()->minimumSize().height());
    }
    if (m_dialog->layout()) {
        m_dialog->layout()->invalidate();
        m_dialog->layout()->activate();
    }
    QTimer::singleShot(0, this, &DialogFontScale::updateLayout);
}

void DialogFontScale::rescale()
{
    applySize(m_fontSize->fontSize());
}

void DialogFontScale::updateLayout()
{
    // QMessageBox は文の折り返しと大きさ（固定サイズ）を自分で計算し直す
    if (qobject_cast<QMessageBox*>(m_dialog)) return;
    DialogUtils::fitWrappedLabels(m_dialog);
    ensureContentFits();
}

void DialogFontScale::ensureContentFits()
{
    // 幅が狭いと操作列が二段になるが、最小の高さは一段の操作列で計算されるため、決定ボタンが隠れないよう
    // 二段になった分だけ高くする。推奨の高さまでは広げず、利用者が縮めた高さ（小さい画面に合わせた高さ）を保つ
    QLayout* layout = m_dialog->layout();
    if (!layout) return;
    layout->activate();
    const QSize minimum = m_dialog->minimumSizeHint();
    QSize size = m_dialog->size().expandedTo(minimum);
    if (m_footer && m_footer->geometry().width() > 0) {
        const int wrapped = m_footer->heightForWidth(m_footer->geometry().width()) - m_footer->minimumSize().height();
        if (wrapped > 0) size.setHeight(qMax(size.height(), minimum.height() + wrapped));
    }
    if (size != m_dialog->size()) m_dialog->resize(size);
}

bool DialogFontScale::eventFilter(QObject* watched, QEvent* event)
{
    // QMessageBox はアイコンなどの変更でレイアウトを作り直し、操作列が外れるので入れ直す
    if (watched == m_dialog && m_ownButtons && m_attached && m_hostLayout != m_dialog->layout()
        && (event->type() == QEvent::LayoutRequest || event->type() == QEvent::Resize
            || event->type() == QEvent::Show)) {
        m_attached = false;
        if (attachToLayout()) applySize(m_fontSize->fontSize());
    }
    // QInputDialog は表示時にレイアウトを生成するため、その後で操作列を挿入する。
    if (watched == m_dialog && m_ownButtons && event->type() == QEvent::Show && !m_attached && attachToLayout()) {
        if (!qobject_cast<QMessageBox*>(m_dialog)) DialogUtils::standardizeDialog(m_dialog);
        applySize(m_fontSize->fontSize());
        if (m_sizeSetting) DialogUtils::restoreDialogSize(m_dialog, *m_sizeSetting);
        // 表示時の大きさは操作列を足す前に決まっているので、少なくとも追加後の推奨サイズまで広げる
        m_dialog->resize(m_dialog->size().expandedTo(m_dialog->sizeHint()));
        ensureContentFits();
    }
    if (watched == m_dialog && event->type() == QEvent::Hide && m_sizeSetting)
        DialogUtils::saveDialogSize(m_dialog, *m_sizeSetting);
    if (watched == m_dialog && (event->type() == QEvent::Resize || event->type() == QEvent::Show))
        QTimer::singleShot(0, this, &DialogFontScale::updateLayout);
    return QWidget::eventFilter(watched, event);
}
