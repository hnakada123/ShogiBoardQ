/// @file csalogpanel.cpp
/// @brief CSA通信ログパネルクラスの実装

#include "csalogpanel.h"

#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>

#include "networksettings.h"

namespace {
LogPanel::Names csaNames()
{
    return {QStringLiteral("csaLogView"), QStringLiteral("csaLogToolbar"), QStringLiteral("csaLogWrap"),
            QStringLiteral("csaLogLatest"), QStringLiteral("csaLogCopy"), QStringLiteral("csaLogClear"),
            QStringLiteral("csaCommandInput"), QStringLiteral("csaSendButton")};
}
} // namespace

CsaLogPanel::CsaLogPanel(QWidget* parent)
    : LogPanel(NetworkSettings::csaLogFontSize, NetworkSettings::csaLogWrapLines, csaNames(), parent)
{
    logView()->setAccessibleName(tr("CSA通信ログ"));
    logView()->setPlaceholderText(tr("CSAサーバーとの送受信内容がここに表示されます。"));
    m_connectionStatus = new QLabel(this);
    m_connectionStatus->setObjectName(QStringLiteral("csaConnectionStatus"));
    m_connectionStatus->setContentsMargins(8, 0, 8, 0);
    addToolbarWidget(m_connectionStatus);
    commandInput()->setAccessibleName(tr("CSAコマンド"));
    sendButton()->setToolTip(tr("CSAサーバーへコマンドを送信（Enter）"));
    connect(this, &LogPanel::commandEntered, this, &CsaLogPanel::csaRawCommandRequested);
    setConnected(false);
}

CsaLogPanel::~CsaLogPanel() = default;

void CsaLogPanel::setConnected(bool connected)
{
    m_connected = connected;
    m_connectionStatus->setText(connected ? tr("接続済み") : tr("未接続"));
    commandInput()->setEnabled(connected);
    commandInput()->setPlaceholderText(connected ? tr("コマンドを入力してEnter")
                                                 : tr("CSAサーバーに接続すると送信できます"));
    updateSendButton();
}
