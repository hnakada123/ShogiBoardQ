/// @file menubuttonwidget.cpp
/// @brief メニューボタンウィジェットクラスの実装

#include "menubuttonwidget.h"
#include "buttonstyles.h"

#include <QMouseEvent>
#include <QDrag>
#include <QMimeData>
#include <QApplication>
#include <QRegularExpression>
#include <QResizeEvent>

namespace {
constexpr auto kFavoriteMimeType = "application/x-shogiboardq-menu-action";

QString displayText(QString text)
{
    text.remove(QRegularExpression(QStringLiteral("\\s*\\(&[^)]\\)")));
    // && は表示用の &、単独の & はアクセラレータ。
    text.replace(QStringLiteral("&&"), QString(QChar(0x1f)));
    text.remove(QChar('&'));
    text.replace(QChar(0x1f), QChar('&'));
    return text;
}
}

MenuButtonWidget::MenuButtonWidget(QAction* action, QWidget* parent)
    : QWidget(parent)
    , m_action(action)
{
    setupUi();
    setAcceptDrops(true);
}

QString MenuButtonWidget::actionName() const
{
    return m_action ? m_action->objectName() : QString();
}

void MenuButtonWidget::setCustomizeMode(bool enabled, bool isFavoriteTab, bool isInFavorites)
{
    m_customizeMode = enabled;
    m_isFavoriteTab = isFavoriteTab;
    m_isInFavorites = isInFavorites;
    updateButtonState();
}

QSize MenuButtonWidget::sizeHint() const
{
    return QSize(m_buttonWidth, m_buttonHeight);
}

void MenuButtonWidget::updateSizes(int buttonSize, int fontSize, int iconSize)
{
    m_buttonSize = buttonSize;
    m_fontSize = fontSize;
    m_iconSize = iconSize;
    QFont textFont = font();
    textFont.setPixelSize(m_fontSize);
    m_textLabel->setFont(textFont);
    onActionChanged();
}

void MenuButtonWidget::updateGeometryForText()
{
    // 文字を拡大したときも、最低8文字分の幅と全文を収める高さを確保する。
    m_buttonWidth = qMax(m_buttonSize, m_fontSize * 8 + 24);
    const int textWidth = m_buttonWidth - 24;
    const QFontMetrics metrics(m_textLabel->font());
    const int textHeight = qMax(metrics.lineSpacing() * 2,
        metrics.boundingRect(QRect(0, 0, textWidth, 10000),
                             Qt::TextWordWrap, m_textLabel->text()).height());
    m_naturalHeight = qMax(m_buttonSize * 4 / 5, m_iconSize + textHeight + 30);
    m_buttonHeight = qMax(m_naturalHeight, m_rowHeight);
    m_textLabel->setFixedSize(textWidth, textHeight);
    m_iconLabel->setFixedSize(m_iconSize, m_iconSize);
    m_mainButton->setFixedSize(m_buttonWidth - 4, m_buttonHeight - 4);
    setFixedSize(m_buttonWidth, m_buttonHeight);
    updateGeometry();
}

void MenuButtonWidget::setRowHeight(int height)
{
    if (m_rowHeight == height) return;
    m_rowHeight = height;
    updateGeometryForText();
}

void MenuButtonWidget::setupUi()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(2, 2, 2, 2);

    m_mainButton = new QPushButton(this);
    m_mainButton->setProperty("automationAction", actionName());
    m_mainButton->setFlat(true);
    m_mainButton->setStyleSheet(ButtonStyles::menuMainButton());
    m_mainButton->installEventFilter(this);
    setFocusProxy(m_mainButton);

    auto* btnLayout = new QVBoxLayout(m_mainButton);
    btnLayout->setContentsMargins(10, 10, 10, 10);
    btnLayout->setSpacing(6);

    m_iconLabel = new QLabel(m_mainButton);
    m_iconLabel->setAlignment(Qt::AlignCenter);
    m_iconLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    btnLayout->addWidget(m_iconLabel, 0, Qt::AlignCenter);

    m_textLabel = new QLabel(m_mainButton);
    m_textLabel->setObjectName(QStringLiteral("menuActionText"));
    m_textLabel->setTextFormat(Qt::PlainText);
    m_textLabel->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
    m_textLabel->setWordWrap(true);
    m_textLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_textLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #344351; } QLabel:disabled { color: #929eaa; }"));
    btnLayout->addWidget(m_textLabel, 0, Qt::AlignCenter);
    // 高さを揃えて余った分は下に回し、同じ行のボタンでアイコンの位置を揃える
    btnLayout->addStretch(1);
    m_mainLayout->addWidget(m_mainButton);

    m_checkedLabel = new QLabel(QStringLiteral("✓"), m_mainButton);
    m_checkedLabel->setObjectName(QStringLiteral("menuActionChecked"));
    m_checkedLabel->setStyleSheet(QStringLiteral("color: #245d87; font-weight: bold;"));
    m_checkedLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_checkedLabel->move(6, 3);
    m_checkedLabel->hide();

    m_addButton = new QPushButton(QStringLiteral("+"), this);
    m_addButton->setObjectName(QStringLiteral("menuAddFavorite"));
    m_addButton->setToolTip(tr("お気に入りに追加"));
    m_addButton->setAccessibleName(m_addButton->toolTip());
    m_addButton->setFixedSize(22, 22);
    m_addButton->setStyleSheet(ButtonStyles::menuAddButton());
    m_addButton->hide();

    m_removeButton = new QPushButton(QStringLiteral("×"), this);
    m_removeButton->setObjectName(QStringLiteral("menuRemoveFavorite"));
    m_removeButton->setToolTip(tr("お気に入りから削除"));
    m_removeButton->setAccessibleName(m_removeButton->toolTip());
    m_removeButton->setFixedSize(22, 22);
    m_removeButton->setStyleSheet(ButtonStyles::menuRemoveButton());
    m_removeButton->hide();

    connect(m_mainButton, &QPushButton::clicked, this, &MenuButtonWidget::onMainButtonClicked);
    connect(m_addButton, &QPushButton::clicked, this, &MenuButtonWidget::onAddButtonClicked);
    connect(m_removeButton, &QPushButton::clicked, this, &MenuButtonWidget::onRemoveButtonClicked);
    if (m_action) {
        connect(m_action, &QAction::changed, this, &MenuButtonWidget::onActionChanged);
        connect(m_action, &QObject::destroyed, this, &MenuButtonWidget::onActionChanged);
    }
    updateSizes(m_buttonSize, m_fontSize, m_iconSize);
}

void MenuButtonWidget::updateButtonState()
{
    m_addButton->setVisible(m_customizeMode && !m_isFavoriteTab && !m_isInFavorites);
    m_removeButton->setVisible(m_customizeMode && m_isFavoriteTab);
    m_addButton->move(width() - m_addButton->width(), 0);
    m_removeButton->move(width() - m_removeButton->width(), 0);
    m_addButton->raise();
    m_removeButton->raise();
    m_mainButton->setEnabled(m_action && (m_customizeMode || m_action->isEnabled()));
    m_mainButton->setCursor(m_customizeMode && m_isFavoriteTab ? Qt::OpenHandCursor : Qt::ArrowCursor);
}

void MenuButtonWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    updateButtonState();
}

void MenuButtonWidget::onMainButtonClicked()
{
    // QPushButton 自身のトグルは採用せず、QAction の状態に従う。
    m_mainButton->setChecked(m_action && m_action->isChecked());
    if (m_action && !m_customizeMode && m_action->isEnabled()) {
        Q_EMIT actionTriggered(m_action);
    } else if (m_action && m_customizeMode && !m_isFavoriteTab && !m_isInFavorites) {
        Q_EMIT addToFavorites(actionName());
    }
}

void MenuButtonWidget::onAddButtonClicked()
{
    if (m_action) Q_EMIT addToFavorites(actionName());
}

void MenuButtonWidget::onRemoveButtonClicked()
{
    if (m_action) Q_EMIT removeFromFavorites(actionName());
}

void MenuButtonWidget::onActionChanged()
{
    if (!m_action) {
        m_mainButton->setEnabled(false);
        return;
    }
    const QString text = displayText(m_action->text());
    m_textLabel->setText(text);
    m_mainButton->setAccessibleName(text);
    QString tooltip = text;
    if (!m_action->shortcut().isEmpty())
        tooltip += QStringLiteral(" (%1)").arg(m_action->shortcut().toString(QKeySequence::NativeText));
    if (!m_action->toolTip().isEmpty() && displayText(m_action->toolTip()) != text)
        tooltip += QLatin1Char('\n') + displayText(m_action->toolTip());
    setToolTip(QStringLiteral("<qt>%1</qt>").arg(tooltip.toHtmlEscaped().replace(QLatin1Char('\n'), QStringLiteral("<br>"))));
    m_mainButton->setToolTip(toolTip());
    m_mainButton->setCheckable(m_action->isCheckable());
    m_mainButton->setChecked(m_action->isChecked());
    m_checkedLabel->setVisible(m_action->isCheckable() && m_action->isChecked());
    const auto mode = m_action->isEnabled() ? QIcon::Normal : QIcon::Disabled;
    m_iconLabel->setPixmap(m_action->icon().pixmap(m_iconSize, m_iconSize, mode));
    updateGeometryForText();
    updateButtonState();
}

bool MenuButtonWidget::eventFilter(QObject* watched, QEvent* event)
{
    // QPushButton が受け取るマウス操作からも、お気に入りをドラッグできるようにする。
    if (watched == m_mainButton && m_customizeMode && m_isFavoriteTab) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto* mouse = static_cast<QMouseEvent*>(event);
            if (mouse->button() == Qt::LeftButton) {
                m_dragStartPosition = mouse->pos();
                m_dragPending = true;
            }
        } else if (event->type() == QEvent::MouseMove) {
            auto* mouse = static_cast<QMouseEvent*>(event);
            if (m_dragPending && (mouse->buttons() & Qt::LeftButton)
                && (mouse->pos() - m_dragStartPosition).manhattanLength() >= QApplication::startDragDistance()) {
                m_dragPending = false;
                m_mainButton->setDown(false);
                startDrag();
                return true;
            }
        } else if (event->type() == QEvent::MouseButtonRelease) {
            m_dragPending = false;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void MenuButtonWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && m_customizeMode && m_isFavoriteTab)
        m_dragStartPosition = event->pos();
    QWidget::mousePressEvent(event);
}

void MenuButtonWidget::mouseMoveEvent(QMouseEvent* event)
{
    if ((event->buttons() & Qt::LeftButton) && m_customizeMode && m_isFavoriteTab
        && (event->pos() - m_dragStartPosition).manhattanLength() >= QApplication::startDragDistance())
        startDrag();
}

void MenuButtonWidget::startDrag()
{
    QPointer<QDrag> drag = new QDrag(this);
    auto* mimeData = new QMimeData;
    mimeData->setData(kFavoriteMimeType, actionName().toUtf8());
    drag->setMimeData(mimeData);
    drag->setPixmap(grab());
    Q_EMIT dragStarted(actionName());
    drag->exec(Qt::MoveAction);
    if (drag) drag->deleteLater();
}

void MenuButtonWidget::dragEnterEvent(QDragEnterEvent* event)
{
    if (m_customizeMode && m_isFavoriteTab && event->mimeData()->hasFormat(kFavoriteMimeType)) {
        event->acceptProposedAction();
        m_mainButton->setDown(true);
    }
}

void MenuButtonWidget::dragLeaveEvent(QDragLeaveEvent* event)
{
    m_mainButton->setDown(false);
    event->accept();
}

void MenuButtonWidget::dropEvent(QDropEvent* event)
{
    m_mainButton->setDown(false);
    if (m_customizeMode && m_isFavoriteTab && event->mimeData()->hasFormat(kFavoriteMimeType)) {
        const QString sourceAction = QString::fromUtf8(event->mimeData()->data(kFavoriteMimeType));
        event->acceptProposedAction();
        if (sourceAction != actionName()) Q_EMIT dropReceived(sourceAction, actionName());
    }
}
