/// @file usilogpanel.cpp
/// @brief USI通信ログパネルクラスの実装

#include "usilogpanel.h"
#include "logviewfontmanager.h"
#include "buttonstyles.h"

#include <QWidget>
#include <QPlainTextEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QToolButton>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QTextCursor>
#include <QTextCharFormat>
#include <QSizePolicy>

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

    QTextCursor cursor = m_logView->textCursor();
    cursor.movePosition(QTextCursor::End);

    QTextCharFormat coloredFormat;
    coloredFormat.setForeground(lineColor);
    cursor.insertText(logLine + QStringLiteral("\n"), coloredFormat);

    m_logView->setTextCursor(cursor);
    m_logView->ensureCursorVisible();
}

void UsiLogPanel::appendStatus(const QString& message)
{
    if (!m_logView || message.isEmpty()) return;

    QTextCursor cursor = m_logView->textCursor();
    cursor.movePosition(QTextCursor::End);

    QTextCharFormat grayFormat;
    grayFormat.setForeground(QColor(0x80, 0x80, 0x80));
    cursor.insertText(QStringLiteral("⚙ ") + message + QStringLiteral("\n"), grayFormat);

    m_logView->setTextCursor(cursor);
    m_logView->ensureCursorVisible();
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
            this, &UsiLogPanel::onFontDecrease);

    m_btnFontIncrease = new QToolButton(m_toolbar);
    m_btnFontIncrease->setText(QStringLiteral("A+"));
    m_btnFontIncrease->setToolTip(tr("フォントサイズを大きくする"));
    m_btnFontIncrease->setFixedSize(28, 24);
    m_btnFontIncrease->setStyleSheet(ButtonStyles::fontButton());
    connect(m_btnFontIncrease, &QToolButton::clicked,
            this, &UsiLogPanel::onFontIncrease);

    m_engine1Label = new QLabel(QStringLiteral("E1: ---"), m_toolbar);
    m_engine1Label->setStyleSheet(QStringLiteral("QLabel { color: #2060a0; font-weight: bold; }"));
    m_engine2Label = new QLabel(QStringLiteral("E2: ---"), m_toolbar);
    m_engine2Label->setStyleSheet(QStringLiteral("QLabel { color: #a02060; font-weight: bold; }"));

    toolbarLayout->addWidget(m_btnFontDecrease);
    toolbarLayout->addWidget(m_btnFontIncrease);
    toolbarLayout->addSpacing(20);
    toolbarLayout->addWidget(m_engine1Label);
    toolbarLayout->addSpacing(20);
    toolbarLayout->addWidget(m_engine2Label);
    toolbarLayout->addStretch();

    m_toolbar->setLayout(toolbarLayout);
    relaxToolbarWidth(m_toolbar);
}

void UsiLogPanel::buildCommandBar()
{
    m_commandBar = new QWidget(m_container);
    auto* layout = new QHBoxLayout(m_commandBar);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    m_targetCombo = new QComboBox(m_commandBar);
    m_targetCombo->addItem(QStringLiteral("E1"), 0);
    m_targetCombo->addItem(QStringLiteral("E2"), 1);
    m_targetCombo->addItem(QStringLiteral("E1+E2"), 2);
    m_targetCombo->setMinimumWidth(70);
    m_targetCombo->setToolTip(tr("コマンドの送信先を選択"));

    m_commandInput = new QLineEdit(m_commandBar);
    m_commandInput->setPlaceholderText(tr("USIコマンドを入力してEnter"));

    layout->addWidget(m_targetCombo);
    layout->addWidget(m_commandInput, 1);

    {
        QFont font;
        font.setPointSize(m_fontSize);
        m_targetCombo->setFont(font);
        m_commandInput->setFont(font);
    }

    m_commandBar->setLayout(layout);
    relaxToolbarWidth(m_commandBar);

    connect(m_commandInput, &QLineEdit::returnPressed,
            this, &UsiLogPanel::onCommandEntered);
}

// ===================== フォント管理 =====================

void UsiLogPanel::initFontManager()
{
    m_fontSize = AnalysisSettings::usiLogFontSize();
    m_fontManager = std::make_unique<LogViewFontManager>(m_fontSize, m_logView);
    m_fontManager->setPostApplyCallback([this](int size) {
        QFont font;
        font.setPointSize(size);
        if (m_targetCombo) m_targetCombo->setFont(font);
        if (m_commandInput) m_commandInput->setFont(font);
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
}

void UsiLogPanel::onEngine1NameChanged()
{
    if (m_engine1Label && m_log1) {
        QString name = m_log1->engineName();
        if (name.isEmpty()) name = QStringLiteral("---");
        m_engine1Label->setText(QStringLiteral("E1: %1").arg(name));
    }
}

void UsiLogPanel::onEngine2NameChanged()
{
    if (m_engine2Label && m_log2) {
        QString name = m_log2->engineName();
        if (name.isEmpty()) name = QStringLiteral("---");
        m_engine2Label->setText(QStringLiteral("E2: %1").arg(name));
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
