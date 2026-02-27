/// @file csalogpanel.cpp
/// @brief CSA通信ログパネルクラスの実装

#include "csalogpanel.h"
#include "logviewfontmanager.h"
#include "buttonstyles.h"

#include <QWidget>
#include <QPlainTextEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QToolButton>
#include <QPushButton>
#include <QLineEdit>
#include <QTextCursor>
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
    layout->setSpacing(2);

    buildToolbar();
    layout->addWidget(m_toolbar);

    buildCommandBar();
    layout->addWidget(m_commandBar);

    m_logView = new QPlainTextEdit(m_container);
    m_logView->setReadOnly(true);
    layout->addWidget(m_logView);

    initFontManager();

    return m_container;
}

void CsaLogPanel::append(const QString& line)
{
    if (m_logView) {
        m_logView->appendPlainText(line);
        QTextCursor cursor = m_logView->textCursor();
        cursor.movePosition(QTextCursor::End);
        m_logView->setTextCursor(cursor);
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
    toolbarLayout->addStretch();

    m_toolbar->setLayout(toolbarLayout);
    relaxToolbarWidth(m_toolbar);
}

void CsaLogPanel::buildCommandBar()
{
    m_commandBar = new QWidget(m_container);
    auto* layout = new QHBoxLayout(m_commandBar);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    m_btnSendToServer = new QPushButton(tr("CSAサーバーへ送信"), m_commandBar);
    m_btnSendToServer->setEnabled(false);
    m_btnSendToServer->setFlat(true);
    m_btnSendToServer->setMinimumWidth(130);

    layout->addWidget(m_btnSendToServer);

    m_commandInput = new QLineEdit(m_commandBar);
    m_commandInput->setPlaceholderText(tr("コマンドを入力してEnter"));
    layout->addWidget(m_commandInput, 1);

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
    if (!m_commandInput) return;

    QString command = m_commandInput->text().trimmed();
    if (command.isEmpty()) return;

    emit csaRawCommandRequested(command);
    m_commandInput->clear();
}
