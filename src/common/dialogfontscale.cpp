#include "dialogfontscale.h"

#include "appsettings.h"
#include "dialogutils.h"

#include <QBoxLayout>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QEvent>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
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
};

void collectFonts(QWidget* root, QList<QPair<QWidget*, QFont>>& fonts)
{
    fonts.append({root, root->font()});
    const auto children = root->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly);
    for (auto* child : children) {
        if (child->isWindow() || child->property("dialogFontScaleExcluded").toBool()) continue;
        collectFonts(child, fonts);
    }
}
}

void DialogFontScale::install(QDialog* dialog, const QString& settingsId, bool saveSize)
{
    new DialogFontScale(dialog, settingsId, saveSize);
}

DialogFontScale::DialogFontScale(QDialog* dialog, const QString& settingsId, bool saveSize)
    : QWidget(dialog), m_dialog(dialog), m_settingsId(settingsId),
      m_sizeLabel(new QLabel(this)), m_size(qMax(8, dialog->font().pointSize())), m_baseSize(m_size), m_saveSize(saveSize)
{
    setObjectName(QStringLiteral("dialogFontScale"));
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    auto* row = new QHBoxLayout(this);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(6);
    auto* decreaseButton = new QPushButton(QStringLiteral("A-"), this);
    auto* increaseButton = new QPushButton(QStringLiteral("A+"), this);
    decreaseButton->setObjectName(QStringLiteral("fontDecrease"));
    increaseButton->setObjectName(QStringLiteral("fontIncrease"));
    m_sizeLabel->setObjectName(QStringLiteral("dialogFontSize"));
    m_sizeLabel->setAlignment(Qt::AlignCenter);
    m_sizeLabel->setToolTip(QCoreApplication::translate("DialogFontScale", "この画面の文字サイズ"));
    row->addWidget(decreaseButton);
    row->addWidget(m_sizeLabel);
    row->addWidget(increaseButton);
    row->addStretch();
    connect(decreaseButton, &QPushButton::clicked, this, &DialogFontScale::decrease);
    connect(increaseButton, &QPushButton::clicked, this, &DialogFontScale::increase);

    if (attachToLayout()) {
        DialogUtils::standardizeDialog(dialog);
        applySize(AppSettings::dialogFontSize(settingsId, m_size));
    }
    if (saveSize) DialogUtils::restoreDialogSize(dialog, AppSettings::auxiliaryDialogSize(settingsId));
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
    } else if (auto* grid = qobject_cast<QGridLayout*>(m_dialog->layout()))
        grid->addWidget(this, grid->rowCount(), 0, 1, grid->columnCount());
    else return false;
    m_attached = true;
    return true;
}

void DialogFontScale::increase()
{
    if (m_size < 24) applySize(m_size + 1);
}

void DialogFontScale::decrease()
{
    if (m_size > 8) applySize(m_size - 1);
}

void DialogFontScale::applySize(int size)
{
    // 先に収集してから適用し、親からの伝播による二重拡大を防ぐ。
    QList<QPair<QWidget*, QFont>> fonts;
    collectFonts(m_dialog, fonts);
    for (const auto& entry : std::as_const(fonts)) {
        QFont font = entry.second;
        if (!entry.first->property("dialogFontBaseSize").isValid())
            entry.first->setProperty("dialogFontBaseSize", font.pointSize());
        const int baseSize = entry.first->property("dialogFontBaseSize").toInt();
        font.setPointSize(qMax(8, baseSize + size - m_baseSize));
        entry.first->setFont(font);
        if (auto* combo = qobject_cast<QComboBox*>(entry.first)) combo->view()->setFont(font);
    }
    m_size = size;
    m_sizeLabel->setText(QStringLiteral("%1 pt").arg(size));
    DialogUtils::updateFontButtons(m_dialog, size);
    layout()->invalidate();
    setMinimumHeight(layout()->minimumSize().height());
    AppSettings::setDialogFontSize(m_settingsId, size);
    if (m_dialog->layout()) {
        m_dialog->layout()->invalidate();
        m_dialog->layout()->activate();
    }
    QTimer::singleShot(0, this, &DialogFontScale::updateLayout);
}

void DialogFontScale::updateLayout()
{
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
    // QInputDialog は表示時にレイアウトを生成するため、その後で操作列を挿入する。
    if (watched == m_dialog && event->type() == QEvent::Show && !m_attached && attachToLayout()) {
        DialogUtils::standardizeDialog(m_dialog);
        applySize(AppSettings::dialogFontSize(m_settingsId, m_size));
        if (m_saveSize) DialogUtils::restoreDialogSize(m_dialog, AppSettings::auxiliaryDialogSize(m_settingsId));
        // 表示時の大きさは操作列を足す前に決まっているので、少なくとも追加後の推奨サイズまで広げる
        m_dialog->resize(m_dialog->size().expandedTo(m_dialog->sizeHint()));
        ensureContentFits();
    }
    if (watched == m_dialog && event->type() == QEvent::Hide && m_saveSize)
        AppSettings::setAuxiliaryDialogSize(m_settingsId, m_dialog->size());
    if (watched == m_dialog && (event->type() == QEvent::Resize || event->type() == QEvent::Show))
        QTimer::singleShot(0, this, &DialogFontScale::updateLayout);
    return QWidget::eventFilter(watched, event);
}
