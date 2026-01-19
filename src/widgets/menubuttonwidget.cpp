#include "menubuttonwidget.h"

#include <QMouseEvent>
#include <QDrag>
#include <QMimeData>
#include <QApplication>
#include <QHBoxLayout>
#include <QStyle>

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
    m_buttonWidth = buttonSize;
    m_buttonHeight = static_cast<int>(buttonSize * 0.9);  // 高さはやや低め
    m_fontSize = fontSize;
    m_iconSize = iconSize;

    // メインボタンのサイズを更新
    m_mainButton->setFixedSize(m_buttonWidth - 4, m_buttonHeight - 4);

    // アイコンを更新
    if (m_action && !m_action->icon().isNull()) {
        m_iconLabel->setPixmap(m_action->icon().pixmap(m_iconSize, m_iconSize));
    }

    // フォントサイズを更新
    m_textLabel->setStyleSheet(QStringLiteral("font-size: %1px; color: white;").arg(m_fontSize));

    // ウィジェット全体のサイズヒントを更新
    setMinimumSize(m_buttonWidth, m_buttonHeight);
    updateGeometry();
}

void MenuButtonWidget::setupUi()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(2, 2, 2, 2);
    m_mainLayout->setSpacing(2);

    // メインボタン（アイコンとテキストを含むコンテナ）
    m_mainButton = new QPushButton(this);
    m_mainButton->setFixedSize(m_buttonWidth - 4, m_buttonHeight - 4);
    m_mainButton->setFlat(true);
    m_mainButton->setStyleSheet(
        "QPushButton {"
        "  border: none;"
        "  border-radius: 6px;"
        "  background-color: #42A5F5;"
        "  color: white;"
        "  padding: 4px;"
        "}"
        "QPushButton:hover {"
        "  background-color: #1E88E5;"
        "}"
        "QPushButton:pressed {"
        "  background-color: #1565C0;"
        "}"
        "QPushButton:disabled {"
        "  background-color: #B0BEC5;"
        "  color: #ECEFF1;"
        "}"
    );

    // ボタン内のレイアウト
    QVBoxLayout* btnLayout = new QVBoxLayout(m_mainButton);
    btnLayout->setContentsMargins(2, 4, 2, 4);
    btnLayout->setSpacing(2);

    // アイコンラベル
    m_iconLabel = new QLabel(m_mainButton);
    m_iconLabel->setAlignment(Qt::AlignCenter);
    if (m_action && !m_action->icon().isNull()) {
        m_iconLabel->setPixmap(m_action->icon().pixmap(m_iconSize, m_iconSize));
    }
    btnLayout->addWidget(m_iconLabel, 0, Qt::AlignCenter);

    // テキストラベル
    m_textLabel = new QLabel(m_mainButton);
    m_textLabel->setAlignment(Qt::AlignCenter);
    m_textLabel->setWordWrap(true);
    if (m_action) {
        QString text = m_action->text();
        text.remove(QChar('&'));  // アクセラレータを除去
        // テキストが長い場合は短縮
        if (text.length() > 8) {
            text = text.left(7) + QStringLiteral("...");
        }
        m_textLabel->setText(text);
    }
    m_textLabel->setStyleSheet(QStringLiteral("font-size: %1px; color: white;").arg(m_fontSize));
    btnLayout->addWidget(m_textLabel, 0, Qt::AlignCenter);

    m_mainLayout->addWidget(m_mainButton, 0, Qt::AlignCenter);

    // カスタマイズモード用ボタン（初期は非表示）
    // 追加ボタン（カテゴリタブで表示）
    m_addButton = new QPushButton(QStringLiteral("+"), this);
    m_addButton->setFixedSize(18, 18);
    m_addButton->setStyleSheet(
        "QPushButton {"
        "  border: 1px solid #4CAF50;"
        "  border-radius: 9px;"
        "  background-color: #4CAF50;"
        "  color: white;"
        "  font-weight: bold;"
        "  font-size: 12px;"
        "}"
        "QPushButton:hover {"
        "  background-color: #45a049;"
        "}"
    );
    m_addButton->hide();

    // 削除ボタン（お気に入りタブで表示）
    m_removeButton = new QPushButton(QStringLiteral("\u00d7"), this);  // ×
    m_removeButton->setFixedSize(18, 18);
    m_removeButton->setStyleSheet(
        "QPushButton {"
        "  border: 1px solid #f44336;"
        "  border-radius: 9px;"
        "  background-color: #f44336;"
        "  color: white;"
        "  font-weight: bold;"
        "  font-size: 12px;"
        "}"
        "QPushButton:hover {"
        "  background-color: #da190b;"
        "}"
    );
    m_removeButton->hide();

    connect(m_mainButton, &QPushButton::clicked, this, &MenuButtonWidget::onMainButtonClicked);
    connect(m_addButton, &QPushButton::clicked, this, &MenuButtonWidget::onAddButtonClicked);
    connect(m_removeButton, &QPushButton::clicked, this, &MenuButtonWidget::onRemoveButtonClicked);

    // アクションの有効/無効状態を反映
    if (m_action) {
        m_mainButton->setEnabled(m_action->isEnabled());
        connect(m_action, &QAction::changed, this, [this]() {
            m_mainButton->setEnabled(m_action->isEnabled());
        });
    }
}

void MenuButtonWidget::updateButtonState()
{
    // カスタマイズモード時のボタン表示を更新
    if (m_customizeMode) {
        if (m_isFavoriteTab) {
            // お気に入りタブでは削除ボタンを表示
            m_removeButton->show();
            m_removeButton->move(width() - m_removeButton->width() - 2, 2);
            m_addButton->hide();
        } else {
            // カテゴリタブでは、まだお気に入りに未登録なら追加ボタンを表示
            if (!m_isInFavorites) {
                m_addButton->show();
                m_addButton->move(2, 2);
            } else {
                m_addButton->hide();
            }
            m_removeButton->hide();
        }
    } else {
        m_addButton->hide();
        m_removeButton->hide();
    }
}

void MenuButtonWidget::onMainButtonClicked()
{
    if (m_action && !m_customizeMode) {
        Q_EMIT actionTriggered(m_action);
    }
}

void MenuButtonWidget::onAddButtonClicked()
{
    if (m_action) {
        Q_EMIT addToFavorites(m_action->objectName());
    }
}

void MenuButtonWidget::onRemoveButtonClicked()
{
    if (m_action) {
        Q_EMIT removeFromFavorites(m_action->objectName());
    }
}

void MenuButtonWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && m_customizeMode && m_isFavoriteTab) {
        m_dragStartPosition = event->pos();
    }
    QWidget::mousePressEvent(event);
}

void MenuButtonWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (!(event->buttons() & Qt::LeftButton))
        return;

    if (!m_customizeMode || !m_isFavoriteTab)
        return;

    if ((event->pos() - m_dragStartPosition).manhattanLength()
        < QApplication::startDragDistance())
        return;

    // ドラッグ開始
    QDrag* drag = new QDrag(this);
    QMimeData* mimeData = new QMimeData;
    mimeData->setText(actionName());
    drag->setMimeData(mimeData);

    Q_EMIT dragStarted(actionName());

    drag->exec(Qt::MoveAction);
}

void MenuButtonWidget::dragEnterEvent(QDragEnterEvent* event)
{
    if (m_customizeMode && m_isFavoriteTab && event->mimeData()->hasText()) {
        event->acceptProposedAction();
        setStyleSheet("border: 2px dashed #2196F3;");
    }
}

void MenuButtonWidget::dropEvent(QDropEvent* event)
{
    if (m_customizeMode && m_isFavoriteTab && event->mimeData()->hasText()) {
        QString sourceAction = event->mimeData()->text();
        if (sourceAction != actionName()) {
            Q_EMIT dropReceived(sourceAction, actionName());
        }
        event->acceptProposedAction();
    }
    setStyleSheet("");
}

void MenuButtonWidget::enterEvent(QEnterEvent* event)
{
    Q_UNUSED(event);
    if (m_action && !m_action->toolTip().isEmpty()) {
        setToolTip(m_action->toolTip());
    } else if (m_action) {
        setToolTip(m_action->text().remove(QChar('&')));
    }
}

void MenuButtonWidget::leaveEvent(QEvent* event)
{
    Q_UNUSED(event);
    setStyleSheet("");
}
