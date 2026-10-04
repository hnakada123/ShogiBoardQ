/// @file usilogpanel.cpp
/// @brief USI通信ログパネルクラスの実装

#include "usilogpanel.h"
#include "logviewfontmanager.h"
#include "buttonstyles.h"
#include "applicationfonts.h"
#include "elidelabel.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QWidget>
#include <QPlainTextEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QToolButton>
#include <QToolBar>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QTextCursor>
#include <QTextCharFormat>
#include <QSizePolicy>
#include <QScrollBar>

#include "analysissettings.h"
#include "usicommlogmodel.h"

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

UsiLogPanel::UsiLogPanel(QObject* parent)
    : QObject(parent)
{
}

UsiLogPanel::~UsiLogPanel() = default;

QWidget* UsiLogPanel::buildUi(QWidget* parent)
{
    m_container = new QWidget(parent);
    auto* layout = new QVBoxLayout(m_container);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    buildToolbar();
    layout->addWidget(m_toolbar);

    m_logView = new QPlainTextEdit(m_container);
    m_logView->setObjectName(QStringLiteral("usiLogView"));
    m_logView->setAccessibleName(tr("USI通信ログ"));
    m_logView->setReadOnly(true);
    // 対局・検討を繰り返しても通信履歴でメモリーが増え続けないようにする。
    m_logView->setMaximumBlockCount(5000);
    m_logView->setFont(ApplicationFonts::monospaceFont());
    m_logView->setLineWrapMode(m_wrapAction->isChecked()
                                  ? QPlainTextEdit::WidgetWidth : QPlainTextEdit::NoWrap);
    m_logView->setPlaceholderText(tr("エンジンとの通信ログを表示します（▶ 送信 / ◀ 受信）"));
    m_logView->document()->setDocumentMargin(6);
    layout->addWidget(m_logView, 1);
    connect(m_logView, &QPlainTextEdit::textChanged, this, &UsiLogPanel::updateLogActions);

    buildCommandBar();
    layout->addWidget(m_commandBar);

    initFontManager();
    updateLogActions();

    return m_container;
}

void UsiLogPanel::setModels(UsiCommLogModel* log1, UsiCommLogModel* log2)
{
    m_log1 = log1;
    m_log2 = log2;

    if (m_log1) {
        connect(m_log1, &UsiCommLogModel::usiCommLogChanged,
                this, &UsiLogPanel::onLog1Changed, Qt::UniqueConnection);
        connect(m_log1, &UsiCommLogModel::engineNameChanged,
                this, &UsiLogPanel::onEngine1NameChanged, Qt::UniqueConnection);
        onEngine1NameChanged();
    }
    if (m_log2) {
        connect(m_log2, &UsiCommLogModel::usiCommLogChanged,
                this, &UsiLogPanel::onLog2Changed, Qt::UniqueConnection);
        connect(m_log2, &UsiCommLogModel::engineNameChanged,
                this, &UsiLogPanel::onEngine2NameChanged, Qt::UniqueConnection);
        onEngine2NameChanged();
    }
}

void UsiLogPanel::appendColoredLog(const QString& logLine, const QColor& lineColor)
{
    if (!m_logView || logLine.isEmpty()) return;

    // 表示用カーソルを動かさず追記する。過去ログの閲覧位置と選択範囲を保つ。
    auto* vertical = m_logView->verticalScrollBar();
    auto* horizontal = m_logView->horizontalScrollBar();
    const int oldVertical = vertical->value();
    const int oldHorizontal = horizontal->value();
    const QTextCursor original = m_logView->textCursor();
    // 数値の位置ではなくカーソルで保持し、先頭ログの削除に追従させる。
    // 末尾を選択している場合も追記で選択が伸びないよう、両端を個別に固定する。
    QTextCursor position(m_logView->document());
    position.setPosition(original.position());
    position.setKeepPositionOnInsert(true);
    QTextCursor anchor(m_logView->document());
    anchor.setPosition(original.anchor());
    anchor.setKeepPositionOnInsert(true);
    const bool follow = oldVertical == vertical->maximum() && !original.hasSelection();

    QTextCursor cursor(m_logView->document());
    cursor.movePosition(QTextCursor::End);

    QTextCharFormat coloredFormat;
    coloredFormat.setForeground(lineColor);
    cursor.beginEditBlock();
    if (!m_logView->document()->isEmpty()) cursor.insertBlock();
    cursor.insertText(logLine, coloredFormat);
    cursor.endEditBlock();

    const int adjustedVertical = vertical->value();
    QTextCursor restored(m_logView->document());
    restored.setPosition(anchor.position());
    restored.setPosition(position.position(), QTextCursor::KeepAnchor);
    m_logView->setTextCursor(restored);
    vertical->setValue(follow ? vertical->maximum() : adjustedVertical);
    horizontal->setValue(oldHorizontal);
}

void UsiLogPanel::appendStatus(const QString& message)
{
    if (message.isEmpty()) return;
    appendColoredLog(QStringLiteral("⚙ ") + message, QColor(0x66, 0x66, 0x66));
}

void UsiLogPanel::clear()
{
    if (m_logView) {
        m_logView->clear();
    }
}

// ===================== ツールバー構築 =====================

void UsiLogPanel::buildToolbar()
{
    // 幅が狭いドックでは標準の拡張メニューへ操作を退避する。
    m_toolbar = new QToolBar(m_container);
    m_toolbar->setObjectName(QStringLiteral("usiLogToolbar"));
    m_toolbar->setStyleSheet(ButtonStyles::panelToolButton());

    m_btnFontDecrease = new QToolButton(m_toolbar);
    m_btnFontDecrease->setText(QStringLiteral("A-"));
    m_btnFontDecrease->setToolTip(tr("フォントサイズを小さくする"));
    m_btnFontDecrease->setFixedSize(28, 24);
    m_btnFontDecrease->setAccessibleName(m_btnFontDecrease->toolTip());
    connect(m_btnFontDecrease, &QToolButton::clicked,
            this, &UsiLogPanel::onFontDecrease);

    m_btnFontIncrease = new QToolButton(m_toolbar);
    m_btnFontIncrease->setText(QStringLiteral("A+"));
    m_btnFontIncrease->setToolTip(tr("フォントサイズを大きくする"));
    m_btnFontIncrease->setFixedSize(28, 24);
    m_btnFontIncrease->setAccessibleName(m_btnFontIncrease->toolTip());
    connect(m_btnFontIncrease, &QToolButton::clicked,
            this, &UsiLogPanel::onFontIncrease);

    m_engine1Label = new ElideLabel(m_toolbar);
    m_engine1Label->setFullText(QStringLiteral("E1: ---"));
    m_engine1Label->setStyleSheet(QStringLiteral("QLabel { color: #2060a0; font-weight: bold; }"));
    m_engine2Label = new ElideLabel(m_toolbar);
    m_engine2Label->setFullText(QStringLiteral("E2: ---"));
    m_engine2Label->setStyleSheet(QStringLiteral("QLabel { color: #a02060; font-weight: bold; }"));
    for (auto* label : {m_engine1Label, m_engine2Label}) {
        label->setElideMode(Qt::ElideRight);
        label->setMaximumWidth(220);
        label->setMinimumWidth(50);
        label->setContentsMargins(6, 0, 6, 0);
    }

    m_toolbar->addWidget(m_btnFontDecrease);
    m_toolbar->addWidget(m_btnFontIncrease);
    m_toolbar->addSeparator();
    m_toolbar->addWidget(m_engine1Label);
    m_toolbar->addWidget(m_engine2Label);
    auto* spacer = new QWidget(m_toolbar);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_toolbar->addWidget(spacer);

    m_wrapAction = m_toolbar->addAction(tr("折り返し"));
    m_wrapAction->setObjectName(QStringLiteral("usiLogWrap"));
    m_wrapAction->setToolTip(tr("長い行をウィンドウ幅で折り返す"));
    m_wrapAction->setCheckable(true);
    m_wrapAction->setChecked(AnalysisSettings::usiLogWrapLines());
    connect(m_wrapAction, &QAction::toggled, this, &UsiLogPanel::onWrapToggled);

    m_latestAction = m_toolbar->addAction(tr("最新へ"));
    m_latestAction->setObjectName(QStringLiteral("usiLogLatest"));
    m_latestAction->setToolTip(tr("最新のログへ移動（末尾では新着ログに自動追従）"));
    connect(m_latestAction, &QAction::triggered, this, &UsiLogPanel::scrollToLatest);
    m_toolbar->addSeparator();
    m_copyAction = m_toolbar->addAction(tr("全てコピー"));
    m_copyAction->setObjectName(QStringLiteral("usiLogCopy"));
    m_copyAction->setToolTip(tr("ログ全体をクリップボードへコピー"));
    connect(m_copyAction, &QAction::triggered, this, &UsiLogPanel::copyAll);
    m_clearAction = m_toolbar->addAction(tr("消去"));
    m_clearAction->setObjectName(QStringLiteral("usiLogClear"));
    m_clearAction->setToolTip(tr("表示中のログを消去"));
    connect(m_clearAction, &QAction::triggered, this, &UsiLogPanel::clear);
    relaxToolbarWidth(m_toolbar);
}

void UsiLogPanel::buildCommandBar()
{
    m_commandBar = new QWidget(m_container);
    auto* layout = new QHBoxLayout(m_commandBar);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    m_targetCombo = new QComboBox(m_commandBar);
    m_targetCombo->setObjectName(QStringLiteral("usiCommandTarget"));
    m_targetCombo->addItem(QStringLiteral("E1"), 0);
    m_targetCombo->addItem(QStringLiteral("E2"), 1);
    m_targetCombo->addItem(QStringLiteral("E1+E2"), 2);
    m_targetCombo->setMinimumWidth(70);
    m_targetCombo->setToolTip(tr("コマンドの送信先を選択"));
    m_targetCombo->setAccessibleName(tr("送信先"));
    m_targetCombo->setCurrentIndex(AnalysisSettings::usiLogCommandTarget());
    auto* targetLabel = new QLabel(tr("送信先"), m_commandBar);
    targetLabel->setBuddy(m_targetCombo);

    m_commandInput = new QLineEdit(m_commandBar);
    m_commandInput->setObjectName(QStringLiteral("usiCommandInput"));
    m_commandInput->setAccessibleName(tr("USIコマンド"));
    m_commandInput->setPlaceholderText(tr("USIコマンドを入力してEnter"));
    m_commandInput->setClearButtonEnabled(true);
    m_commandInput->setFont(ApplicationFonts::monospaceFont());

    m_sendButton = new QPushButton(tr("送信"), m_commandBar);
    m_sendButton->setObjectName(QStringLiteral("usiCommandSend"));
    m_sendButton->setToolTip(tr("選択した送信先へUSIコマンドを送信"));
    m_sendButton->setEnabled(false);

    layout->addWidget(targetLabel);
    layout->addWidget(m_targetCombo);
    layout->addWidget(m_commandInput, 1);
    layout->addWidget(m_sendButton);

    m_commandBar->setLayout(layout);
    relaxToolbarWidth(m_commandBar);

    connect(m_commandInput, &QLineEdit::returnPressed,
            this, &UsiLogPanel::onCommandEntered);
    connect(m_sendButton, &QPushButton::clicked, this, &UsiLogPanel::onCommandEntered);
    connect(m_commandInput, &QLineEdit::textChanged, this, &UsiLogPanel::updateSendButton);
    connect(m_targetCombo, &QComboBox::currentIndexChanged, this, &UsiLogPanel::onTargetChanged);
}

// ===================== フォント管理 =====================

void UsiLogPanel::initFontManager()
{
    m_fontSize = qBound(8, AnalysisSettings::usiLogFontSize(), 24);
    m_fontManager = std::make_unique<LogViewFontManager>(m_fontSize, m_logView);
    m_fontManager->setPostApplyCallback([this](int size) {
        QFont font = m_commandInput->font();
        font.setPointSize(size);
        m_commandInput->setFont(font);
        m_btnFontDecrease->setEnabled(size > 8);
        m_btnFontIncrease->setEnabled(size < 24);
        AnalysisSettings::setUsiLogFontSize(size);
    });
    m_fontManager->apply();
}

void UsiLogPanel::onFontIncrease()
{
    m_fontManager->increase();
}

void UsiLogPanel::onFontDecrease()
{
    m_fontManager->decrease();
}

// ===================== イベント処理 =====================

void UsiLogPanel::onCommandEntered()
{
    if (!m_commandInput || !m_targetCombo) return;

    QString command = m_commandInput->text().trimmed();
    if (command.isEmpty()) return;

    int target = m_targetCombo->currentData().toInt();
    emit usiCommandRequested(target, command);

    m_commandInput->clear();
    m_commandInput->setFocus();
}

void UsiLogPanel::onEngine1NameChanged()
{
    if (m_engine1Label && m_log1) {
        QString name = m_log1->engineName();
        if (name.isEmpty()) name = QStringLiteral("---");
        m_engine1Label->setFullText(QStringLiteral("E1: %1").arg(name));
    }
}

void UsiLogPanel::onEngine2NameChanged()
{
    if (m_engine2Label && m_log2) {
        QString name = m_log2->engineName();
        if (name.isEmpty()) name = QStringLiteral("---");
        m_engine2Label->setFullText(QStringLiteral("E2: %1").arg(name));
    }
}

void UsiLogPanel::onLog1Changed()
{
    if (m_logView && m_log1) {
        appendColoredLog(m_log1->usiCommLog(), QColor(0x20, 0x60, 0xa0));
    }
}

void UsiLogPanel::onLog2Changed()
{
    if (m_logView && m_log2) {
        appendColoredLog(m_log2->usiCommLog(), QColor(0xa0, 0x20, 0x60));
    }
}

void UsiLogPanel::onWrapToggled(bool checked)
{
    if (m_logView) {
        auto* scroll = m_logView->verticalScrollBar();
        const bool follow = scroll->value() == scroll->maximum() && !m_logView->textCursor().hasSelection();
        m_logView->setLineWrapMode(checked ? QPlainTextEdit::WidgetWidth : QPlainTextEdit::NoWrap);
        if (follow) scroll->setValue(scroll->maximum());
    }
    AnalysisSettings::setUsiLogWrapLines(checked);
}

void UsiLogPanel::scrollToLatest()
{
    QTextCursor cursor = m_logView->textCursor();
    cursor.clearSelection();
    m_logView->setTextCursor(cursor);
    m_logView->verticalScrollBar()->setValue(m_logView->verticalScrollBar()->maximum());
}

void UsiLogPanel::copyAll()
{
    QApplication::clipboard()->setText(m_logView->toPlainText());
}

void UsiLogPanel::updateLogActions()
{
    const bool hasLog = !m_logView->document()->isEmpty();
    m_copyAction->setEnabled(hasLog);
    m_clearAction->setEnabled(hasLog);
    m_latestAction->setEnabled(hasLog);
}

void UsiLogPanel::updateSendButton()
{
    m_sendButton->setEnabled(!m_commandInput->text().trimmed().isEmpty());
}

void UsiLogPanel::onTargetChanged(int index)
{
    AnalysisSettings::setUsiLogCommandTarget(index);
}
