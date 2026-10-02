/// @file csalogpanel.cpp
/// @brief CSA通信ログパネルクラスの実装

#include "csalogpanel.h"
#include "logviewfontmanager.h"
#include "buttonstyles.h"
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

CsaLogPanel::CsaLogPanel(QObject* parent)
    : QObject(parent)
{
}

CsaLogPanel::~CsaLogPanel() = default;

QWidget* CsaLogPanel::buildUi(QWidget* parent)
{
    m_container = new QWidget(parent);
    auto* layout = new QVBoxLayout(m_container);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(6);

    buildToolbar();
    layout->addWidget(m_toolbar);

    m_logView = new QPlainTextEdit(m_container);
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
    initFontManager();

    return m_container;
}

void CsaLogPanel::append(const QString& line)
{
    if (m_logView) {
        m_logView->appendPlainText(line);
    }
}

void CsaLogPanel::clear()
{
    if (m_logView) {
        m_logView->clear();
    }
}

// ===================== ツールバー構築 =====================

void CsaLogPanel::buildToolbar()
{
    m_toolbar = new QWidget(m_container);
    auto* toolbarLayout = new QHBoxLayout(m_toolbar);
    toolbarLayout->setContentsMargins(2, 2, 2, 2);
    toolbarLayout->setSpacing(4);

    m_btnFontDecrease = new QToolButton(m_toolbar);
    m_btnFontDecrease->setText(QStringLiteral("A-"));
    m_btnFontDecrease->setToolTip(tr("フォントサイズを小さくする"));
    m_btnFontDecrease->setFixedSize(28, 24);
    m_btnFontDecrease->setStyleSheet(ButtonStyles::fontButton());
    connect(m_btnFontDecrease, &QToolButton::clicked,
            this, &CsaLogPanel::onFontDecrease);

    m_btnFontIncrease = new QToolButton(m_toolbar);
    m_btnFontIncrease->setText(QStringLiteral("A+"));
    m_btnFontIncrease->setToolTip(tr("フォントサイズを大きくする"));
    m_btnFontIncrease->setFixedSize(28, 24);
    m_btnFontIncrease->setStyleSheet(ButtonStyles::fontButton());
    connect(m_btnFontIncrease, &QToolButton::clicked,
            this, &CsaLogPanel::onFontIncrease);

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
    m_commandBar = new QWidget(m_container);
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
        cmdFont.setPointSize(m_fontSize);
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
    if (m_connectionStatus) m_connectionStatus->setText(connected ? tr("接続済み") : tr("未接続"));
    if (m_commandInput) {
        m_commandInput->setEnabled(connected);
        m_commandInput->setPlaceholderText(connected ? tr("コマンドを入力してEnter")
                                                    : tr("CSAサーバーに接続すると送信できます"));
    }
    updateSendButton();
}

void CsaLogPanel::updateSendButton()
{
    if (m_btnSendToServer && m_commandInput) {
        m_btnSendToServer->setEnabled(m_connected && !m_commandInput->text().trimmed().isEmpty());
    }
}

void CsaLogPanel::copyLog()
{
    if (m_logView) QApplication::clipboard()->setText(m_logView->toPlainText());
}

// ===================== フォント管理 =====================

void CsaLogPanel::initFontManager()
{
    m_fontSize = NetworkSettings::csaLogFontSize();
    m_fontManager = std::make_unique<LogViewFontManager>(m_fontSize, m_logView);
    m_fontManager->setPostApplyCallback([this](int size) {
        QFont font;
        font.setPointSize(size);
        if (m_btnSendToServer) m_btnSendToServer->setFont(font);
        if (m_commandInput) m_commandInput->setFont(font);
        NetworkSettings::setCsaLogFontSize(size);
    });
    m_fontManager->apply();
}

void CsaLogPanel::onFontIncrease()
{
    m_fontManager->increase();
}

void CsaLogPanel::onFontDecrease()
{
    m_fontManager->decrease();
}

// ===================== イベント処理 =====================

void CsaLogPanel::onCommandEntered()
{
    if (!m_connected || !m_commandInput) return;

    QString command = m_commandInput->text().trimmed();
    if (command.isEmpty()) return;

    emit csaRawCommandRequested(command);
    m_commandInput->clear();
}
