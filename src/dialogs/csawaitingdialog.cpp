/// @file csawaitingdialog.cpp
/// @brief CSA接続待機ダイアログクラスの実装

#include "csawaitingdialog.h"

#include <QLabel>
#include <QPushButton>
#include <QToolButton>
#include <QProgressBar>
#include <QPlainTextEdit>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTextCursor>
#include "loggingcategory.h"

#include "settingsservice.h"  // フォントサイズ保存用

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
    , m_btnLogFontIncrease(nullptr)
    , m_btnLogFontDecrease(nullptr)
    , m_logFontSize(SettingsService::csaLogFontSize())  // SettingsServiceから読み込み
    , m_btnSendToServer(nullptr)
    , m_commandInput(nullptr)
{
    qCDebug(lcUi) << "Constructor called, coordinator=" << coordinator;
    setupUi();

    // ログウィンドウを事前に作成（シグナル受信前に準備しておく）
    createLogWindow();

    connectSignalsAndSlots();

    // 初期状態を設定
    if (m_coordinator) {
        m_statusLabel->setText(getStateMessage(m_coordinator->gameState()));
        qCDebug(lcUi) << "Initial state:" << static_cast<int>(m_coordinator->gameState());
    }
}

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
    statusFont.setPointSize(12);
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
    qCDebug(lcUi) << "connectSignalsAndSlots called";

    // キャンセルボタン
    connect(m_cancelButton, &QPushButton::clicked,
            this, &CsaWaitingDialog::onCancelClicked);

    // 通信ログボタン
    connect(m_showLogButton, &QPushButton::clicked,
            this, &CsaWaitingDialog::onShowLogClicked);

    // コーディネータからのシグナル
    if (m_coordinator) {
        qCDebug(lcUi) << "Connecting to coordinator signals...";

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

        qCDebug(lcUi) << "All signals connected";
    } else {
        qCWarning(lcUi) << "coordinator is null!";
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

    // === フォントサイズ調整ボタン ===
    QHBoxLayout* fontLayout = new QHBoxLayout();
    fontLayout->addStretch();

    m_btnLogFontDecrease = new QToolButton(m_logWindow);
    m_btnLogFontDecrease->setText(QStringLiteral("A-"));
    m_btnLogFontDecrease->setToolTip(tr("フォントサイズを小さくする"));
    m_btnLogFontDecrease->setFixedSize(28, 24);
    connect(m_btnLogFontDecrease, &QToolButton::clicked,
            this, &CsaWaitingDialog::onLogFontDecrease);
    fontLayout->addWidget(m_btnLogFontDecrease);

    m_btnLogFontIncrease = new QToolButton(m_logWindow);
    m_btnLogFontIncrease->setText(QStringLiteral("A+"));
    m_btnLogFontIncrease->setToolTip(tr("フォントサイズを大きくする"));
    m_btnLogFontIncrease->setFixedSize(28, 24);
    connect(m_btnLogFontIncrease, &QToolButton::clicked,
            this, &CsaWaitingDialog::onLogFontIncrease);
    fontLayout->addWidget(m_btnLogFontIncrease);

    layout->addLayout(fontLayout);

    // === コマンド入力エリア ===
    QHBoxLayout* commandLayout = new QHBoxLayout();

    // CSAサーバーへ送信ボタン（ラベル表示用）
    m_btnSendToServer = new QPushButton(tr("CSAサーバーへ送信"), m_logWindow);
    m_btnSendToServer->setEnabled(false);  // クリック不可（ラベルとして表示）
    m_btnSendToServer->setFlat(true);      // フラットスタイル
    m_btnSendToServer->setMinimumWidth(130);

    commandLayout->addWidget(m_btnSendToServer);

    // コマンド入力欄
    m_commandInput = new QLineEdit(m_logWindow);
    m_commandInput->setPlaceholderText(tr("コマンドを入力してEnter"));
    commandLayout->addWidget(m_commandInput, 1);  // stretchで伸縮

    // コマンド入力部分のフォントサイズを設定
    {
        QFont cmdFont;
        cmdFont.setPointSize(m_logFontSize);
        m_btnSendToServer->setFont(cmdFont);
        m_commandInput->setFont(cmdFont);
    }

    layout->addLayout(commandLayout);

    // === ログ表示エリア ===
    m_logTextEdit = new QPlainTextEdit(m_logWindow);
    m_logTextEdit->setReadOnly(true);
    m_logTextEdit->setLineWrapMode(QPlainTextEdit::NoWrap);
    // メモリリーク防止：ログの最大行数を制限（古いログは自動削除される）
    m_logTextEdit->setMaximumBlockCount(5000);

    // 固定幅フォントを設定
    QFont font = m_logTextEdit->font();
    font.setFamily(QStringLiteral("monospace"));
    font.setStyleHint(QFont::Monospace);
    font.setPointSize(m_logFontSize);
    m_logTextEdit->setFont(font);

    layout->addWidget(m_logTextEdit);

    // === 閉じるボタン ===
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    QPushButton* closeButton = new QPushButton(tr("閉じる"), m_logWindow);
    closeButton->setAutoDefault(false);  // Enterキーで反応しないように
    closeButton->setDefault(false);
    closeButton->setFocusPolicy(Qt::NoFocus);  // フォーカスを受けないように
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

// ログウィンドウのフォントサイズを更新
void CsaWaitingDialog::updateLogFontSize(int delta)
{
    m_logFontSize += delta;
    if (m_logFontSize < 8) m_logFontSize = 8;
    if (m_logFontSize > 24) m_logFontSize = 24;

    // ログ表示エリア
    if (m_logTextEdit) {
        QFont font = m_logTextEdit->font();
        font.setPointSize(m_logFontSize);
        m_logTextEdit->setFont(font);
    }

    // コマンド入力部分も同じフォントサイズに
    if (m_btnSendToServer) {
        QFont font = m_btnSendToServer->font();
        font.setPointSize(m_logFontSize);
        m_btnSendToServer->setFont(font);
    }

    if (m_commandInput) {
        QFont font = m_commandInput->font();
        font.setPointSize(m_logFontSize);
        m_commandInput->setFont(font);
    }

    // SettingsServiceに保存
    SettingsService::setCsaLogFontSize(m_logFontSize);
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
    qCDebug(lcUi) << "onCsaCommLogAppended received:" << line;
    qCDebug(lcUi) << "m_logTextEdit=" << m_logTextEdit;

    // ログウィンドウが作成されていれば追記
    if (m_logTextEdit) {
        m_logTextEdit->appendPlainText(line);
        // 自動スクロール
        QTextCursor cursor = m_logTextEdit->textCursor();
        cursor.movePosition(QTextCursor::End);
        m_logTextEdit->setTextCursor(cursor);
        qCDebug(lcUi) << "Log appended to text edit";
    } else {
        qCWarning(lcUi) << "m_logTextEdit is null, log not displayed";
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

    // CSAサーバーへ送信
    m_coordinator->sendRawCommand(command);

    // 入力欄をクリア
    m_commandInput->clear();
}

// ログウィンドウのフォントサイズを大きくする
void CsaWaitingDialog::onLogFontIncrease()
{
    updateLogFontSize(1);
}

// ログウィンドウのフォントサイズを小さくする
void CsaWaitingDialog::onLogFontDecrease()
{
    updateLogFontSize(-1);
}
