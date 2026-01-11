#include "csawaitingdialog.h"

#include <QLabel>
#include <QPushButton>
#include <QProgressBar>
#include <QPlainTextEdit>
#include <QLineEdit>
#include <QButtonGroup>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTextCursor>
#include <QDebug>

// コンストラクタ
CsaWaitingDialog::CsaWaitingDialog(CsaGameCoordinator* coordinator, QWidget* parent)
    : QDialog(parent)
    , m_coordinator(coordinator)
    , m_statusLabel(nullptr)
    , m_detailLabel(nullptr)
    , m_progressBar(nullptr)
    , m_cancelButton(nullptr)
    , m_showLogButton(nullptr)
    , m_logWindow(nullptr)
    , m_logTextEdit(nullptr)
    , m_senderButtonGroup(nullptr)
    , m_btnSenderServer(nullptr)
    , m_btnSenderClient(nullptr)
    , m_commandInput(nullptr)
{
    qDebug() << "[CsaWaitingDialog] Constructor called, coordinator=" << coordinator;
    setupUi();

    // ログウィンドウを事前に作成（シグナル受信前に準備しておく）
    createLogWindow();

    connectSignalsAndSlots();

    // 初期状態を設定
    if (m_coordinator) {
        m_statusLabel->setText(getStateMessage(m_coordinator->gameState()));
        qDebug() << "[CsaWaitingDialog] Initial state:" << static_cast<int>(m_coordinator->gameState());
    }
}

// デストラクタ
CsaWaitingDialog::~CsaWaitingDialog()
{
    // 通信ログウィンドウを閉じる
    if (m_logWindow) {
        m_logWindow->close();
        delete m_logWindow;
        m_logWindow = nullptr;
    }
}

// UIの初期化
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
    m_statusLabel->setAlignment(Qt::AlignCenter);
    QFont statusFont = m_statusLabel->font();
    statusFont.setPointSize(statusFont.pointSize() + 2);
    statusFont.setBold(true);
    m_statusLabel->setFont(statusFont);
    mainLayout->addWidget(m_statusLabel);

    // プログレスバー（不定表示）
    m_progressBar = new QProgressBar(this);
    m_progressBar->setMinimum(0);
    m_progressBar->setMaximum(0);  // 不定表示モード
    m_progressBar->setTextVisible(false);
    mainLayout->addWidget(m_progressBar);

    // 詳細メッセージラベル
    m_detailLabel = new QLabel(this);
    m_detailLabel->setAlignment(Qt::AlignCenter);
    m_detailLabel->setWordWrap(true);
    m_detailLabel->setStyleSheet(QStringLiteral("color: gray;"));
    mainLayout->addWidget(m_detailLabel);

    // スペーサー
    mainLayout->addStretch();

    // ボタンレイアウト
    QVBoxLayout* buttonLayout = new QVBoxLayout();
    buttonLayout->setSpacing(10);

    // 通信ログボタン
    QHBoxLayout* logButtonLayout = new QHBoxLayout();
    logButtonLayout->addStretch();
    m_showLogButton = new QPushButton(tr("通信ログ..."), this);
    m_showLogButton->setMinimumWidth(120);
    logButtonLayout->addWidget(m_showLogButton);
    logButtonLayout->addStretch();
    buttonLayout->addLayout(logButtonLayout);

    // キャンセルボタン
    QHBoxLayout* cancelButtonLayout = new QHBoxLayout();
    cancelButtonLayout->addStretch();
    m_cancelButton = new QPushButton(tr("対局キャンセル"), this);
    m_cancelButton->setMinimumWidth(120);
    cancelButtonLayout->addWidget(m_cancelButton);
    cancelButtonLayout->addStretch();
    buttonLayout->addLayout(cancelButtonLayout);

    mainLayout->addLayout(buttonLayout);

    setLayout(mainLayout);
}

// シグナル・スロットの接続
void CsaWaitingDialog::connectSignalsAndSlots()
{
    qDebug() << "[CsaWaitingDialog] connectSignalsAndSlots called";

    // キャンセルボタン
    connect(m_cancelButton, &QPushButton::clicked,
            this, &CsaWaitingDialog::onCancelClicked);

    // 通信ログボタン
    connect(m_showLogButton, &QPushButton::clicked,
            this, &CsaWaitingDialog::onShowLogClicked);

    // コーディネータからのシグナル
    if (m_coordinator) {
        qDebug() << "[CsaWaitingDialog] Connecting to coordinator signals...";

        // gameStateChanged シグナルを接続
        connect(m_coordinator, &CsaGameCoordinator::gameStateChanged,
                this, &CsaWaitingDialog::onGameStateChanged);

        // エラーシグナル
        connect(m_coordinator, &CsaGameCoordinator::errorOccurred,
                this, &CsaWaitingDialog::onErrorOccurred);

        // ログメッセージシグナル
        connect(m_coordinator, &CsaGameCoordinator::logMessage,
                this, &CsaWaitingDialog::onLogMessage);

        // CSA通信ログシグナル
        connect(m_coordinator, &CsaGameCoordinator::csaCommLogAppended,
                this, &CsaWaitingDialog::onCsaCommLogAppended);

        qDebug() << "[CsaWaitingDialog] All signals connected";
    } else {
        qDebug() << "[CsaWaitingDialog] WARNING: coordinator is null!";
    }
}

// 状態に応じたメッセージを取得
QString CsaWaitingDialog::getStateMessage(CsaGameCoordinator::GameState state) const
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

// 通信ログウィンドウを作成
void CsaWaitingDialog::createLogWindow()
{
    if (m_logWindow) {
        return;  // 既に作成済み
    }

    m_logWindow = new QDialog(this);
    m_logWindow->setWindowTitle(tr("CSA通信ログ"));
    m_logWindow->setWindowFlags(m_logWindow->windowFlags() & ~Qt::WindowContextHelpButtonHint);
    m_logWindow->resize(550, 450);

    QVBoxLayout* layout = new QVBoxLayout(m_logWindow);

    // === コマンド入力エリア ===
    QHBoxLayout* commandLayout = new QHBoxLayout();

    // 送信元選択ボタングループ
    m_senderButtonGroup = new QButtonGroup(m_logWindow);

    // CSAサーバーボタン
    m_btnSenderServer = new QPushButton(tr("▶ CSAサーバー"), m_logWindow);
    m_btnSenderServer->setCheckable(true);
    m_btnSenderServer->setMinimumWidth(100);
    m_senderButtonGroup->addButton(m_btnSenderServer, 0);

    // GUI（クライアント）ボタン - ユーザー名を表示
    QString clientName = tr("◀ %1").arg(
        m_coordinator ? m_coordinator->username() : tr("GUI"));
    m_btnSenderClient = new QPushButton(clientName, m_logWindow);
    m_btnSenderClient->setCheckable(true);
    m_btnSenderClient->setChecked(true);  // デフォルトでGUI側を選択
    m_btnSenderClient->setMinimumWidth(100);
    m_senderButtonGroup->addButton(m_btnSenderClient, 1);

    commandLayout->addWidget(m_btnSenderServer);
    commandLayout->addWidget(m_btnSenderClient);

    // コマンド入力欄
    m_commandInput = new QLineEdit(m_logWindow);
    m_commandInput->setPlaceholderText(tr("コマンドを入力してEnter"));
    commandLayout->addWidget(m_commandInput, 1);  // stretchで伸縮

    layout->addLayout(commandLayout);

    // === ログ表示エリア ===
    m_logTextEdit = new QPlainTextEdit(m_logWindow);
    m_logTextEdit->setReadOnly(true);
    m_logTextEdit->setLineWrapMode(QPlainTextEdit::NoWrap);

    // 固定幅フォントを設定
    QFont font = m_logTextEdit->font();
    font.setFamily(QStringLiteral("monospace"));
    font.setStyleHint(QFont::Monospace);
    m_logTextEdit->setFont(font);

    layout->addWidget(m_logTextEdit);

    // === 閉じるボタン ===
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    QPushButton* closeButton = new QPushButton(tr("閉じる"), m_logWindow);
    connect(closeButton, &QPushButton::clicked,
            m_logWindow, &QDialog::close);
    buttonLayout->addWidget(closeButton);
    buttonLayout->addStretch();
    layout->addLayout(buttonLayout);

    m_logWindow->setLayout(layout);

    // コマンド入力のEnterキー処理を接続
    connect(m_commandInput, &QLineEdit::returnPressed,
            this, &CsaWaitingDialog::onCommandEntered);
}

// 対局状態変化時の処理
void CsaWaitingDialog::onGameStateChanged(CsaGameCoordinator::GameState state)
{
    m_statusLabel->setText(getStateMessage(state));

    // 対局開始またはエラー時はダイアログを閉じる
    if (state == CsaGameCoordinator::GameState::InGame) {
        // 通信ログウィンドウも閉じる
        if (m_logWindow) {
            m_logWindow->close();
        }
        accept();  // 正常終了としてダイアログを閉じる
    } else if (state == CsaGameCoordinator::GameState::Error ||
               state == CsaGameCoordinator::GameState::Idle) {
        // エラーまたは中断時はダイアログを閉じる
        if (m_logWindow) {
            m_logWindow->close();
        }
        reject();
    }
}

// エラー発生時の処理
void CsaWaitingDialog::onErrorOccurred(const QString& message)
{
    m_detailLabel->setText(message);
    m_detailLabel->setStyleSheet(QStringLiteral("color: red;"));
}

// ログメッセージ受信時の処理
void CsaWaitingDialog::onLogMessage(const QString& message, bool isError)
{
    m_detailLabel->setText(message);
    if (isError) {
        m_detailLabel->setStyleSheet(QStringLiteral("color: red;"));
    } else {
        m_detailLabel->setStyleSheet(QStringLiteral("color: gray;"));
    }
}

// キャンセルボタン押下時の処理
void CsaWaitingDialog::onCancelClicked()
{
    // 通信ログウィンドウを閉じる
    if (m_logWindow) {
        m_logWindow->close();
    }
    emit cancelRequested();
    reject();  // ダイアログを閉じる
}

// 通信ログボタン押下時の処理
void CsaWaitingDialog::onShowLogClicked()
{
    // ログウィンドウを作成（未作成の場合）
    createLogWindow();

    // ウィンドウを表示
    if (m_logWindow) {
        m_logWindow->show();
        m_logWindow->raise();
        m_logWindow->activateWindow();
    }
}

// CSA通信ログ受信時の処理
void CsaWaitingDialog::onCsaCommLogAppended(const QString& line)
{
    qDebug() << "[CsaWaitingDialog] onCsaCommLogAppended received:" << line;
    qDebug() << "[CsaWaitingDialog] m_logTextEdit=" << m_logTextEdit;

    // ログウィンドウが作成されていれば追記
    if (m_logTextEdit) {
        m_logTextEdit->appendPlainText(line);
        // 自動スクロール
        QTextCursor cursor = m_logTextEdit->textCursor();
        cursor.movePosition(QTextCursor::End);
        m_logTextEdit->setTextCursor(cursor);
        qDebug() << "[CsaWaitingDialog] Log appended to text edit";
    } else {
        qDebug() << "[CsaWaitingDialog] WARNING: m_logTextEdit is null, log not displayed";
    }
}

// コマンド入力でEnterが押された時の処理
void CsaWaitingDialog::onCommandEntered()
{
    if (!m_commandInput || !m_coordinator) {
        return;
    }

    QString command = m_commandInput->text().trimmed();
    if (command.isEmpty()) {
        return;
    }

    // 送信元に応じて処理を分岐
    int senderId = m_senderButtonGroup->checkedId();

    if (senderId == 0) {
        // CSAサーバーからの受信をシミュレート
        m_coordinator->simulateServerMessage(command);
    } else {
        // GUI側からサーバーへ送信
        m_coordinator->sendRawCommand(command);
    }

    // 入力欄をクリア
    m_commandInput->clear();
}
