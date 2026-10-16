/// @file csalogpanel.cpp
/// @brief CSA通信ログパネルクラスの実装

#include "csalogpanel.h"
#include "fontsizecontrol.h"
#include "applicationfonts.h"

#include <QWidget>
#include <QPlainTextEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QToolButton>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QApplication>
#include <QClipboard>
#include <QSizePolicy>

#include "networksettings.h"

namespace {
void relaxToolbarWidth(QWidget* toolbar)
{
    if (!toolbar) return;
    toolbar->setMinimumWidth(0);
    QSizePolicy pol = toolbar->sizePolicy();
    pol.setHorizontalPolicy(QSizePolicy::Ignored);
    toolbar->setSizePolicy(pol);
}
} // namespace

CsaLogPanel::CsaLogPanel(QWidget* parent)
    : QWidget(parent)
    , m_fontSize(NetworkSettings::csaLogFontSize, 8, 24)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(6);

    buildToolbar();
    layout->addWidget(m_toolbar);

    m_logView = new QPlainTextEdit(this);
    m_logView->setObjectName(QStringLiteral("csaLogView"));
    m_logView->setReadOnly(true);
    m_logView->setFont(ApplicationFonts::monospaceFont());
    m_logView->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_logView->setMaximumBlockCount(5000);
    m_logView->setPlaceholderText(tr("CSAサーバーとの送受信内容がここに表示されます。"));
    layout->addWidget(m_logView);

    buildCommandBar();
    layout->addWidget(m_commandBar);
    setConnected(m_connected);
    applyFontSize();
}

CsaLogPanel::~CsaLogPanel() = default;

void CsaLogPanel::append(const QString& line)
{
    m_logView->appendPlainText(line);
}

void CsaLogPanel::clear()
{
    m_logView->clear();
}

// ===================== ツールバー構築 =====================

void CsaLogPanel::buildToolbar()
{
    m_toolbar = new QWidget(this);
    auto* toolbarLayout = new QHBoxLayout(m_toolbar);
    toolbarLayout->setContentsMargins(2, 2, 2, 2);
    toolbarLayout->setSpacing(4);

    m_btnFontDecrease = new QToolButton(m_toolbar);
    m_btnFontIncrease = new QToolButton(m_toolbar);
    new FontSizeControl(m_btnFontDecrease, m_btnFontIncrease, &m_fontSize);
    m_btnFontDecrease->setFixedSize(28, 24);
    m_btnFontIncrease->setFixedSize(28, 24);
    connect(&m_fontSize, &FontSizeHelper::fontSizeChanged, this, &CsaLogPanel::applyFontSize);

    toolbarLayout->addWidget(m_btnFontDecrease);
    toolbarLayout->addWidget(m_btnFontIncrease);
    m_connectionStatus = new QLabel(m_toolbar);
    m_connectionStatus->setObjectName(QStringLiteral("csaConnectionStatus"));
    toolbarLayout->addSpacing(8);
    toolbarLayout->addWidget(m_connectionStatus);
    toolbarLayout->addStretch();

    auto* copyButton = new QToolButton(m_toolbar);
    copyButton->setObjectName(QStringLiteral("csaCopyLogButton"));
    copyButton->setText(tr("コピー"));
    copyButton->setToolTip(tr("通信ログ全体をコピー"));
    connect(copyButton, &QToolButton::clicked, this, &CsaLogPanel::copyLog);
    toolbarLayout->addWidget(copyButton);

    auto* clearButton = new QToolButton(m_toolbar);
    clearButton->setObjectName(QStringLiteral("csaClearLogButton"));
    clearButton->setText(tr("クリア"));
    clearButton->setToolTip(tr("表示中の通信ログを消去"));
    connect(clearButton, &QToolButton::clicked, this, &CsaLogPanel::clear);
    toolbarLayout->addWidget(clearButton);

    m_toolbar->setLayout(toolbarLayout);
    relaxToolbarWidth(m_toolbar);
}

void CsaLogPanel::buildCommandBar()
{
    m_commandBar = new QWidget(this);
    auto* layout = new QHBoxLayout(m_commandBar);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    m_commandInput = new QLineEdit(m_commandBar);
    m_commandInput->setObjectName(QStringLiteral("csaCommandInput"));
    m_commandInput->setAccessibleName(tr("CSAコマンド"));
    layout->addWidget(m_commandInput, 1);

    m_btnSendToServer = new QPushButton(tr("送信"), m_commandBar);
    m_btnSendToServer->setObjectName(QStringLiteral("csaSendButton"));
    m_btnSendToServer->setToolTip(tr("CSAサーバーへコマンドを送信（Enter）"));
    layout->addWidget(m_btnSendToServer);

    {
        QFont cmdFont;
        cmdFont.setPointSize(m_fontSize.fontSize());
        m_btnSendToServer->setFont(cmdFont);
        m_commandInput->setFont(cmdFont);
    }

    m_commandBar->setLayout(layout);
    relaxToolbarWidth(m_commandBar);

    connect(m_commandInput, &QLineEdit::returnPressed,
            this, &CsaLogPanel::onCommandEntered);
    connect(m_commandInput, &QLineEdit::textChanged,
            this, &CsaLogPanel::updateSendButton);
    connect(m_btnSendToServer, &QPushButton::clicked,
            this, &CsaLogPanel::onCommandEntered);
}

void CsaLogPanel::setConnected(bool connected)
{
    m_connected = connected;
    m_connectionStatus->setText(connected ? tr("接続済み") : tr("未接続"));
    m_commandInput->setEnabled(connected);
    m_commandInput->setPlaceholderText(connected ? tr("コマンドを入力してEnter")
                                                : tr("CSAサーバーに接続すると送信できます"));
    updateSendButton();
}

void CsaLogPanel::updateSendButton()
{
    m_btnSendToServer->setEnabled(m_connected && !m_commandInput->text().trimmed().isEmpty());
}

void CsaLogPanel::copyLog()
{
    QApplication::clipboard()->setText(m_logView->toPlainText());
}

// ===================== フォント管理 =====================

void CsaLogPanel::applyFontSize()
{
    const int size = m_fontSize.fontSize();
    QFont logFont = m_logView->font();
    logFont.setPointSize(size);
    m_logView->setFont(logFont);
    QFont font;
    font.setPointSize(size);
    m_btnSendToServer->setFont(font);
    m_commandInput->setFont(font);
}

// ===================== イベント処理 =====================

void CsaLogPanel::onCommandEntered()
{
    if (!m_connected) return;

    QString command = m_commandInput->text().trimmed();
    if (command.isEmpty()) return;

    emit csaRawCommandRequested(command);
    m_commandInput->clear();
}
