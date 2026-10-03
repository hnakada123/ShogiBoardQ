/// @file csawaitingdialog.cpp
/// @brief CSA接続待機ダイアログクラスの実装

#include "csawaitingdialog.h"
#include "buttonstyles.h"
#include "applicationfonts.h"
#include "dialogutils.h"

#include <QLabel>
#include <QPushButton>
#include <QToolButton>
#include <QProgressBar>
#include <QPlainTextEdit>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include "logcategories.h"

#include "networksettings.h"

namespace {
constexpr QSize kLogWindowDefaultSize{550, 450};
} // namespace

CsaWaitingDialog::CsaWaitingDialog(CsaGameCoordinator* coordinator, QWidget* parent)
    : QDialog(parent)
    , m_coordinator(coordinator)
    , m_fontHelper({NetworkSettings::csaWaitingDialogFontSize(), 8, 24, 1,
                    NetworkSettings::setCsaWaitingDialogFontSize})
    , m_logFontHelper({NetworkSettings::csaLogFontSize(), 8, 24, 1,
                       NetworkSettings::setCsaLogFontSize})
{
    qCDebug(lcUi) << "Constructor called, coordinator=" << coordinator;
    setupUi();
    applyFontSize();
    DialogUtils::restoreDialogSize(this, NetworkSettings::csaWaitingDialogSize());

    createLogWindow();
    connectSignalsAndSlots();
    if (m_coordinator) {
        connect(m_coordinator, &CsaGameCoordinator::connectionStateChanged,
                this, &CsaWaitingDialog::updateCommandState);
    }
    updateCommandState();
    if (m_coordinator) {
        m_statusLabel->setText(stateMessage(m_coordinator->gameState()));
        qCDebug(lcUi) << "Initial state:" << static_cast<int>(m_coordinator->gameState());
    }
}

CsaWaitingDialog::~CsaWaitingDialog()
{
    DialogUtils::saveDialogSize(this, NetworkSettings::setCsaWaitingDialogSize);
    if (m_logWindow) {
        NetworkSettings::setCsaLogWindowSize(m_logWindow->size());
        m_logWindow->close();
    }
}
void CsaWaitingDialog::setupUi()
{
    setWindowTitle(tr("通信対局（CSA）"));
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    setMinimumWidth(350);
    setModal(true);

    // メインレイアウト
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(15);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    // 状態表示ラベル
    m_statusLabel = new QLabel(tr("対局相手を待機中..."), this);
    m_statusLabel->setObjectName(QStringLiteral("csaWaitingStatus"));
    m_statusLabel->setAlignment(Qt::AlignCenter);
    QFont statusFont = m_statusLabel->font();
    statusFont.setBold(true);
    m_statusLabel->setFont(statusFont);
    mainLayout->addWidget(m_statusLabel);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setMinimum(0);
    m_progressBar->setMaximum(0);
    m_progressBar->setTextVisible(false);
    mainLayout->addWidget(m_progressBar);

    // 詳細メッセージラベル
    m_detailLabel = new QLabel(this);
    m_detailLabel->setObjectName(QStringLiteral("csaWaitingDetail"));
    m_detailLabel->setAlignment(Qt::AlignCenter);
    m_detailLabel->setWordWrap(true);
    m_detailLabel->setStyleSheet(QStringLiteral("color: gray;"));
    mainLayout->addWidget(m_detailLabel);

    mainLayout->addStretch();

    // ボタンレイアウト
    QVBoxLayout* buttonLayout = new QVBoxLayout();
    buttonLayout->setSpacing(10);

    QHBoxLayout* logButtonLayout = new QHBoxLayout();
    logButtonLayout->addStretch();
    m_showLogButton = new QPushButton(tr("通信ログ"), this);
    m_showLogButton->setMinimumWidth(120);
    m_showLogButton->setStyleSheet(ButtonStyles::secondaryNeutral());
    logButtonLayout->addWidget(m_showLogButton);
    logButtonLayout->addStretch();
    buttonLayout->addLayout(logButtonLayout);

    QHBoxLayout* cancelButtonLayout = new QHBoxLayout();
    cancelButtonLayout->addStretch();
    m_cancelButton = new QPushButton(tr("対局キャンセル"), this);
    m_cancelButton->setMinimumWidth(120);
    m_cancelButton->setStyleSheet(ButtonStyles::dangerStop());
    cancelButtonLayout->addWidget(m_cancelButton);
    cancelButtonLayout->addStretch();
    buttonLayout->addLayout(cancelButtonLayout);

    mainLayout->addLayout(buttonLayout);

    QHBoxLayout* fontLayout = new QHBoxLayout();
    fontLayout->addStretch();

    m_btnFontDecrease = new QToolButton(this);
    m_btnFontDecrease->setText(QStringLiteral("A-"));
    m_btnFontDecrease->setToolTip(tr("文字サイズを縮小"));
    m_btnFontDecrease->setStyleSheet(ButtonStyles::fontButton());
    fontLayout->addWidget(m_btnFontDecrease);

    m_btnFontIncrease = new QToolButton(this);
    m_btnFontIncrease->setText(QStringLiteral("A+"));
    m_btnFontIncrease->setToolTip(tr("文字サイズを拡大"));
    m_btnFontIncrease->setStyleSheet(ButtonStyles::fontButton());
    fontLayout->addWidget(m_btnFontIncrease);

    fontLayout->addStretch();
    mainLayout->addLayout(fontLayout);

    setLayout(mainLayout);
}

void CsaWaitingDialog::connectSignalsAndSlots()
{
    qCDebug(lcUi) << "connectSignalsAndSlots called";

    connect(m_cancelButton, &QPushButton::clicked,
            this, &CsaWaitingDialog::onCancelClicked);
    connect(m_showLogButton, &QPushButton::clicked,
            this, &CsaWaitingDialog::onShowLogClicked);
    connect(m_btnFontIncrease, &QToolButton::clicked,
            this, &CsaWaitingDialog::onFontIncrease);
    connect(m_btnFontDecrease, &QToolButton::clicked,
            this, &CsaWaitingDialog::onFontDecrease);

    if (m_coordinator) {
        qCDebug(lcUi) << "Connecting to coordinator signals...";
        connect(m_coordinator, &CsaGameCoordinator::gameStateChanged,
                this, &CsaWaitingDialog::onGameStateChanged);
        connect(m_coordinator, &CsaGameCoordinator::errorOccurred,
                this, &CsaWaitingDialog::onErrorOccurred);
        connect(m_coordinator, &CsaGameCoordinator::logMessage,
                this, &CsaWaitingDialog::onLogMessage);
        connect(m_coordinator, &CsaGameCoordinator::csaCommLogAppended,
                this, &CsaWaitingDialog::onCsaCommLogAppended);

        qCDebug(lcUi) << "All signals connected";
    } else {
        qCWarning(lcUi) << "coordinator is null!";
    }
}

QString CsaWaitingDialog::stateMessage(CsaGameCoordinator::GameState state) const
{
    switch (state) {
    case CsaGameCoordinator::GameState::Idle:
        return tr("待機中...");
    case CsaGameCoordinator::GameState::Connecting:
        return tr("サーバーに接続中...");
    case CsaGameCoordinator::GameState::LoggingIn:
        return tr("ログイン中...");
    case CsaGameCoordinator::GameState::WaitingForMatch:
        return tr("対局相手を待機中...");
    case CsaGameCoordinator::GameState::WaitingForAgree:
        return tr("対局条件を確認中...");
    case CsaGameCoordinator::GameState::InGame:
        return tr("対局開始！");
    case CsaGameCoordinator::GameState::GameOver:
        return tr("対局終了");
    case CsaGameCoordinator::GameState::Error:
        return tr("エラーが発生しました");
    default:
        return tr("不明な状態");
    }
}

void CsaWaitingDialog::createLogWindow()
{
    if (m_logWindow) return;

    m_logWindow = new QDialog(this);
    m_logWindow->setObjectName(QStringLiteral("csaWaitingLogWindow"));
    m_logWindow->setWindowTitle(tr("CSA通信ログ"));
    m_logWindow->setWindowFlags(m_logWindow->windowFlags() & ~Qt::WindowContextHelpButtonHint);
    QSize savedLogSize = NetworkSettings::csaLogWindowSize();
    if (savedLogSize.isValid() && savedLogSize.width() > 100 && savedLogSize.height() > 100) {
        m_logWindow->resize(savedLogSize);
    } else {
        m_logWindow->resize(kLogWindowDefaultSize);
    }

    QVBoxLayout* layout = new QVBoxLayout(m_logWindow);

    QHBoxLayout* commandLayout = new QHBoxLayout();
    m_commandInput = new QLineEdit(m_logWindow);
    m_commandInput->setObjectName(QStringLiteral("csaWaitingCommandInput"));
    m_commandInput->setPlaceholderText(tr("コマンドを入力してEnter"));
    m_commandInput->setAccessibleName(tr("CSAコマンド"));
    commandLayout->addWidget(m_commandInput, 1);
    m_btnSendToServer = new QPushButton(tr("送信"), m_logWindow);
    m_btnSendToServer->setObjectName(QStringLiteral("csaWaitingSendButton"));
    m_btnSendToServer->setAutoDefault(false);
    commandLayout->addWidget(m_btnSendToServer);
    {
        QFont cmdFont;
        cmdFont.setPointSize(m_logFontHelper.fontSize());
        m_btnSendToServer->setFont(cmdFont);
        m_commandInput->setFont(cmdFont);
    }

    m_logTextEdit = new QPlainTextEdit(m_logWindow);
    m_logTextEdit->setObjectName(QStringLiteral("csaWaitingLogView"));
    m_logTextEdit->setReadOnly(true);
    m_logTextEdit->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_logTextEdit->setMaximumBlockCount(5000);
    m_logTextEdit->setPlainText(m_logLines.join(QLatin1Char('\n')));
    QFont font = ApplicationFonts::monospaceFont();
    font.setPointSize(m_logFontHelper.fontSize());
    m_logTextEdit->setFont(font);

    layout->addWidget(m_logTextEdit);
    layout->addLayout(commandLayout);

    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    QFont btnFont;
    btnFont.setPointSize(m_logFontHelper.fontSize());

    m_btnLogFontDecrease = new QToolButton(m_logWindow);
    m_btnLogFontDecrease->setText(QStringLiteral("A-"));
    m_btnLogFontDecrease->setToolTip(tr("フォントサイズを小さくする"));
    m_btnLogFontDecrease->setFont(btnFont);
    m_btnLogFontDecrease->setStyleSheet(ButtonStyles::fontButton());
    connect(m_btnLogFontDecrease, &QToolButton::clicked,
            this, &CsaWaitingDialog::onLogFontDecrease);
    buttonLayout->addWidget(m_btnLogFontDecrease);

    m_btnLogFontIncrease = new QToolButton(m_logWindow);
    m_btnLogFontIncrease->setText(QStringLiteral("A+"));
    m_btnLogFontIncrease->setToolTip(tr("フォントサイズを大きくする"));
    m_btnLogFontIncrease->setFont(btnFont);
    m_btnLogFontIncrease->setStyleSheet(ButtonStyles::fontButton());
    connect(m_btnLogFontIncrease, &QToolButton::clicked,
            this, &CsaWaitingDialog::onLogFontIncrease);
    buttonLayout->addWidget(m_btnLogFontIncrease);

    buttonLayout->addSpacing(10);

    m_logCloseButton = new QPushButton(tr("閉じる"), m_logWindow);
    m_logCloseButton->setAutoDefault(false);
    m_logCloseButton->setDefault(false);
    m_logCloseButton->setFocusPolicy(Qt::StrongFocus);
    m_logCloseButton->setFont(btnFont);
    connect(m_logCloseButton, &QPushButton::clicked,
            m_logWindow, &QDialog::close);
    buttonLayout->addWidget(m_logCloseButton);

    buttonLayout->addStretch();
    layout->addLayout(buttonLayout);

    m_logWindow->setLayout(layout);
    DialogUtils::standardizeDialog(m_logWindow);
    applyLogFontSize();

    connect(m_commandInput, &QLineEdit::returnPressed,
            this, &CsaWaitingDialog::onCommandEntered);
    connect(m_commandInput, &QLineEdit::textChanged,
            this, &CsaWaitingDialog::updateCommandState);
    connect(m_btnSendToServer, &QPushButton::clicked,
            this, &CsaWaitingDialog::onCommandEntered);
}
void CsaWaitingDialog::applyLogFontSize()
{
    const int size = m_logFontHelper.fontSize();

    if (m_logTextEdit) {
        QFont font = m_logTextEdit->font();
        font.setPointSize(size);
        m_logTextEdit->setFont(font);
    }
    if (m_btnSendToServer) {
        QFont font = m_btnSendToServer->font();
        font.setPointSize(size);
        m_btnSendToServer->setFont(font);
    }

    if (m_commandInput) {
        QFont font = m_commandInput->font();
        font.setPointSize(size);
        m_commandInput->setFont(font);
    }

    QFont btnFont;
    btnFont.setPointSize(size);
    if (m_logCloseButton) {
        m_logCloseButton->setFont(btnFont);
    }
    if (m_btnLogFontDecrease) {
        m_btnLogFontDecrease->setFont(btnFont);
    }
    if (m_btnLogFontIncrease) {
        m_btnLogFontIncrease->setFont(btnFont);
    }
    DialogUtils::updateFontButtons(m_logWindow, size);
}

void CsaWaitingDialog::onGameStateChanged(CsaGameCoordinator::GameState state)
{
    m_statusLabel->setText(stateMessage(state));

    if (state == CsaGameCoordinator::GameState::InGame) {
        if (m_logWindow) m_logWindow->close();
        accept();
    } else if (state == CsaGameCoordinator::GameState::Error ||
               state == CsaGameCoordinator::GameState::Idle) {
        if (m_logWindow) m_logWindow->close();
        reject();
    }
}
void CsaWaitingDialog::onErrorOccurred(const QString& message)
{
    m_detailLabel->setText(message);
    m_detailLabel->setStyleSheet(QStringLiteral("color: red;"));
}

void CsaWaitingDialog::onLogMessage(const QString& message, bool isError)
{
    m_detailLabel->setText(message);
    if (isError) {
        m_detailLabel->setStyleSheet(QStringLiteral("color: red;"));
    } else {
        m_detailLabel->setStyleSheet(QStringLiteral("color: gray;"));
    }
}

void CsaWaitingDialog::onCancelClicked()
{
    if (m_logWindow) m_logWindow->close();
    emit cancelRequested();
    reject();
}
void CsaWaitingDialog::onShowLogClicked()
{
    createLogWindow();
    if (m_logWindow) {
        m_logWindow->show();
        m_logWindow->raise();
        m_logWindow->activateWindow();
    }
}

void CsaWaitingDialog::onCsaCommLogAppended(const QString& line)
{
    m_logLines.append(line);
    if (m_logLines.size() > 5000) m_logLines.removeFirst();

    if (m_logTextEdit) {
        m_logTextEdit->appendPlainText(line);
    }
}

void CsaWaitingDialog::onCommandEntered()
{
    if (!m_commandInput || !m_coordinator || !m_coordinator->isConnected()) return;

    QString command = m_commandInput->text().trimmed();
    if (command.isEmpty()) return;
    m_coordinator->sendRawCommand(command);
    m_commandInput->clear();
}

void CsaWaitingDialog::updateCommandState()
{
    const bool connected = m_coordinator && m_coordinator->isConnected();
    m_commandInput->setEnabled(connected);
    m_btnSendToServer->setEnabled(connected && !m_commandInput->text().trimmed().isEmpty());
}
void CsaWaitingDialog::onLogFontIncrease()
{
    if (m_logFontHelper.increase()) applyLogFontSize();
}

void CsaWaitingDialog::onLogFontDecrease()
{
    if (m_logFontHelper.decrease()) applyLogFontSize();
}

void CsaWaitingDialog::onFontIncrease()
{
    if (m_fontHelper.increase()) applyFontSize();
}

void CsaWaitingDialog::onFontDecrease()
{
    if (m_fontHelper.decrease()) applyFontSize();
}

void CsaWaitingDialog::applyFontSize()
{
    DialogUtils::standardizeDialog(this);
    const int size = m_fontHelper.fontSize();
    QFont f = font();
    f.setPointSize(size);
    setFont(f);

    const QList<QWidget*> widgets = findChildren<QWidget*>();
    for (QWidget* widget : std::as_const(widgets)) {
        if (widget && widget->window() == this) {
            widget->setFont(f);
        }
    }

    if (m_statusLabel) {
        QFont boldFont = f;
        boldFont.setBold(true);
        boldFont.setPointSize(size + 2);
        m_statusLabel->setFont(boldFont);
    }
    DialogUtils::updateFontButtons(this, size);
}
