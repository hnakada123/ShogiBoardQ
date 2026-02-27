/// @file csawaitingdialog.cpp
/// @brief CSA接続待機ダイアログクラスの実装

#include "csawaitingdialog.h"
#include "buttonstyles.h"

#include <QLabel>
#include <QPushButton>
#include <QToolButton>
#include <QProgressBar>
#include <QPlainTextEdit>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTextCursor>
#include "logcategories.h"

#include "networksettings.h"  // フォントサイズ保存用

CsaWaitingDialog::CsaWaitingDialog(CsaGameCoordinator* coordinator, QWidget* parent)
    : QDialog(parent)
    , m_coordinator(coordinator)
    , m_fontSize(NetworkSettings::csaWaitingDialogFontSize())
    , m_logFontSize(NetworkSettings::csaLogFontSize())  // SettingsServiceから読み込み
{
    qCDebug(lcUi) << "Constructor called, coordinator=" << coordinator;
    setupUi();
    applyFontSize();

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
    // 通信ログウィンドウのサイズを保存して閉じる
    if (m_logWindow) {
        NetworkSettings::setCsaLogWindowSize(m_logWindow->size());
        m_logWindow->close();
        // m_logWindow は this を parent に持つため、Qt親子モデルにより自動解放される
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
    m_showLogButton = new QPushButton(tr("通信ログ"), this);
    m_showLogButton->setMinimumWidth(120);
    m_showLogButton->setStyleSheet(ButtonStyles::secondaryNeutral());
    logButtonLayout->addWidget(m_showLogButton);
    logButtonLayout->addStretch();
    buttonLayout->addLayout(logButtonLayout);

    // キャンセルボタン
    QHBoxLayout* cancelButtonLayout = new QHBoxLayout();
    cancelButtonLayout->addStretch();
    m_cancelButton = new QPushButton(tr("対局キャンセル"), this);
    m_cancelButton->setMinimumWidth(120);
    m_cancelButton->setStyleSheet(ButtonStyles::dangerStop());
    cancelButtonLayout->addWidget(m_cancelButton);
    cancelButtonLayout->addStretch();
    buttonLayout->addLayout(cancelButtonLayout);

    mainLayout->addLayout(buttonLayout);

    // フォントサイズ調整ボタン（A-/A+）
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

    // フォントサイズ調整ボタン
    connect(m_btnFontIncrease, &QToolButton::clicked,
            this, &CsaWaitingDialog::onFontIncrease);
    connect(m_btnFontDecrease, &QToolButton::clicked,
            this, &CsaWaitingDialog::onFontDecrease);

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
    // ログウィンドウサイズを復元
    QSize savedLogSize = NetworkSettings::csaLogWindowSize();
    if (savedLogSize.isValid() && savedLogSize.width() > 100 && savedLogSize.height() > 100) {
        m_logWindow->resize(savedLogSize);
    } else {
        m_logWindow->resize(550, 450);
    }

    QVBoxLayout* layout = new QVBoxLayout(m_logWindow);

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

    // === フォントサイズ調整ボタン + 閉じるボタン ===
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    // ボタン用フォント
    QFont btnFont;
    btnFont.setPointSize(m_logFontSize);

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
    m_logCloseButton->setAutoDefault(false);  // Enterキーで反応しないように
    m_logCloseButton->setDefault(false);
    m_logCloseButton->setFocusPolicy(Qt::NoFocus);  // フォーカスを受けないように
    m_logCloseButton->setFont(btnFont);
    connect(m_logCloseButton, &QPushButton::clicked,
            m_logWindow, &QDialog::close);
    buttonLayout->addWidget(m_logCloseButton);

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

    // 閉じるボタン・フォントサイズ調整ボタンも連動
    QFont btnFont;
    btnFont.setPointSize(m_logFontSize);
    if (m_logCloseButton) {
        m_logCloseButton->setFont(btnFont);
    }
    if (m_btnLogFontDecrease) {
        m_btnLogFontDecrease->setFont(btnFont);
    }
    if (m_btnLogFontIncrease) {
        m_btnLogFontIncrease->setFont(btnFont);
    }

    // SettingsServiceに保存
    NetworkSettings::setCsaLogFontSize(m_logFontSize);
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

// ダイアログのフォントサイズを大きくする
void CsaWaitingDialog::onFontIncrease()
{
    if (m_fontSize < 24) {
        m_fontSize += 1;
        applyFontSize();
        NetworkSettings::setCsaWaitingDialogFontSize(m_fontSize);
    }
}

// ダイアログのフォントサイズを小さくする
void CsaWaitingDialog::onFontDecrease()
{
    if (m_fontSize > 8) {
        m_fontSize -= 1;
        applyFontSize();
        NetworkSettings::setCsaWaitingDialogFontSize(m_fontSize);
    }
}

// ダイアログのフォントサイズを適用する
void CsaWaitingDialog::applyFontSize()
{
    m_fontSize = qBound(8, m_fontSize, 24);

    QFont f = font();
    f.setPointSize(m_fontSize);
    setFont(f);

    // KDE Breeze対策：全子ウィジェットに明示的にフォントを設定
    const QList<QWidget*> widgets = findChildren<QWidget*>();
    for (QWidget* widget : std::as_const(widgets)) {
        if (widget) {
            widget->setFont(f);
        }
    }

    // ステータスラベルは太字フォントを維持
    if (m_statusLabel) {
        QFont boldFont = f;
        boldFont.setBold(true);
        // フォントサイズに応じてステータスラベルを少し大きく
        boldFont.setPointSize(m_fontSize + 2);
        m_statusLabel->setFont(boldFont);
    }
}
