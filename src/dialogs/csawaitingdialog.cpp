/// @file csawaitingdialog.cpp
/// @brief CSA接続待機ダイアログクラスの実装

#include "csawaitingdialog.h"
#include "dialogfontscale.h"
#include "csalogpanel.h"
#include "buttonstyles.h"
#include "dialogutils.h"

#include <QLabel>
#include <QPushButton>
#include <QToolButton>
#include <QProgressBar>
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
{
    qCDebug(lcUi) << "Constructor called, coordinator=" << coordinator;
    setupUi();
    // 文字の大きさ。状態の表示は本文より 2pt 大きいまま拡大・縮小する
    DialogFontScale::Options fontOptions;
    fontOptions.decrease = m_btnFontDecrease;
    fontOptions.increase = m_btnFontIncrease;
    DialogFontScale::install(this, NetworkSettings::csaWaitingDialogFontSize, fontOptions);
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
    DialogUtils::saveDialogSize(this, NetworkSettings::csaWaitingDialogSize);
    if (m_logWindow) {
        NetworkSettings::csaLogWindowSize.set(m_logWindow->size());
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
    statusFont.setPointSize(statusFont.pointSize() + 2);
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
    fontLayout->addWidget(m_btnFontDecrease);
    m_btnFontIncrease = new QToolButton(this);
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

    // 中身は対局中の CSA 通信ログのドックと同じ部品（文字の大きさの設定も共有する）
    auto* layout = new QVBoxLayout(m_logWindow);
    m_logPanel = new CsaLogPanel(m_logWindow);
    layout->addWidget(m_logPanel);
    connect(m_logPanel, &CsaLogPanel::csaRawCommandRequested, this, &CsaWaitingDialog::onRawCommandRequested);

    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    auto* closeButton = new QPushButton(tr("閉じる"), m_logWindow);
    closeButton->setAutoDefault(false);
    closeButton->setDefault(false);
    connect(closeButton, &QPushButton::clicked, m_logWindow, &QDialog::close);
    buttonLayout->addWidget(closeButton);
    layout->addLayout(buttonLayout);

    DialogUtils::standardizeDialog(m_logWindow);
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
    m_logPanel->append(line);
}

void CsaWaitingDialog::onRawCommandRequested(const QString& command)
{
    if (m_coordinator && m_coordinator->isConnected()) m_coordinator->sendRawCommand(command);
}

void CsaWaitingDialog::updateCommandState()
{
    m_logPanel->setConnected(m_coordinator && m_coordinator->isConnected());
}
