#include "dialogfontscale.h"

#include "appsettings.h"
#include "dialogutils.h"

#include <QBoxLayout>
#include <QCoreApplication>
#include <QDialog>
#include <QEvent>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QComboBox>
#include <QAbstractItemView>

namespace {
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
    if (auto* box = qobject_cast<QBoxLayout*>(m_dialog->layout()))
        box->insertWidget(qMax(0, box->count() - 1), this);
    else if (auto* grid = qobject_cast<QGridLayout*>(m_dialog->layout()))
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
}

bool DialogFontScale::eventFilter(QObject* watched, QEvent* event)
{
    // QInputDialog は表示時にレイアウトを生成するため、その後で操作列を挿入する。
    if (watched == m_dialog && event->type() == QEvent::Show && !m_attached && attachToLayout()) {
        DialogUtils::standardizeDialog(m_dialog);
        applySize(AppSettings::dialogFontSize(m_settingsId, m_size));
        if (m_saveSize) DialogUtils::restoreDialogSize(m_dialog, AppSettings::auxiliaryDialogSize(m_settingsId));
    }
    if (watched == m_dialog && event->type() == QEvent::Hide && m_saveSize)
        AppSettings::setAuxiliaryDialogSize(m_settingsId, m_dialog->size());
    if (watched == m_dialog && (event->type() == QEvent::Resize || event->type() == QEvent::Show))
        QTimer::singleShot(0, this, &DialogFontScale::updateLayout);
    return QWidget::eventFilter(watched, event);
}
