/// @file usilogpanel.cpp
/// @brief USI通信ログパネルクラスの実装

#include "usilogpanel.h"
#include "elidelabel.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>

#include "analysissettings.h"
#include "usicommlogmodel.h"

namespace {
const QColor kEngine1Color(0x20, 0x60, 0xa0);
const QColor kEngine2Color(0xa0, 0x20, 0x60);

LogPanel::Names usiNames()
{
    return {QStringLiteral("usiLogView"), QStringLiteral("usiLogToolbar"), QStringLiteral("usiLogWrap"),
            QStringLiteral("usiLogLatest"), QStringLiteral("usiLogCopy"), QStringLiteral("usiLogClear"),
            QStringLiteral("usiCommandInput"), QStringLiteral("usiCommandSend")};
}
} // namespace

UsiLogPanel::UsiLogPanel(QWidget* parent)
    : LogPanel(AnalysisSettings::usiLogFontSize, AnalysisSettings::usiLogWrapLines, usiNames(), parent)
{
    logView()->setAccessibleName(tr("USI通信ログ"));
    logView()->setPlaceholderText(tr("エンジンとの通信ログを表示します（▶ 送信 / ◀ 受信）"));

    // 操作列のエンジン名（エンジンごとのログの色）
    m_engine1Label = new ElideLabel(this);
    m_engine1Label->setFullText(QStringLiteral("E1: ---"));
    m_engine1Label->setStyleSheet(QStringLiteral("QLabel { color: %1; font-weight: bold; }").arg(kEngine1Color.name()));
    m_engine2Label = new ElideLabel(this);
    m_engine2Label->setFullText(QStringLiteral("E2: ---"));
    m_engine2Label->setStyleSheet(QStringLiteral("QLabel { color: %1; font-weight: bold; }").arg(kEngine2Color.name()));
    for (auto* label : {m_engine1Label, m_engine2Label}) {
        label->setElideMode(Qt::ElideRight);
        label->setMaximumWidth(220);
        label->setMinimumWidth(50);
        label->setContentsMargins(6, 0, 6, 0);
        addToolbarWidget(label);
    }

    // コマンドの送信先
    m_targetCombo = new QComboBox(this);
    m_targetCombo->setObjectName(QStringLiteral("usiCommandTarget"));
    m_targetCombo->addItem(QStringLiteral("E1"), 0);
    m_targetCombo->addItem(QStringLiteral("E2"), 1);
    m_targetCombo->addItem(QStringLiteral("E1+E2"), 2);
    m_targetCombo->setMinimumWidth(70);
    m_targetCombo->setToolTip(tr("コマンドの送信先を選択"));
    m_targetCombo->setAccessibleName(tr("送信先"));
    m_targetCombo->setCurrentIndex(AnalysisSettings::usiLogCommandTarget());
    auto* targetLabel = new QLabel(tr("送信先"), this);
    targetLabel->setBuddy(m_targetCombo);
    commandLayout()->insertWidget(0, targetLabel);
    commandLayout()->insertWidget(1, m_targetCombo);
    commandInput()->setAccessibleName(tr("USIコマンド"));
    commandInput()->setPlaceholderText(tr("USIコマンドを入力してEnter"));
    sendButton()->setToolTip(tr("選択した送信先へUSIコマンドを送信"));

    connect(m_targetCombo, &QComboBox::currentIndexChanged, this, &UsiLogPanel::onTargetChanged);
    connect(this, &LogPanel::commandEntered, this, &UsiLogPanel::sendToTarget);
}

UsiLogPanel::~UsiLogPanel() = default;

void UsiLogPanel::setModels(UsiCommLogModel* log1, UsiCommLogModel* log2)
{
    m_log1 = log1;
    m_log2 = log2;

    if (m_log1) {
        connect(m_log1, &UsiCommLogModel::logLineAppended,
                this, &UsiLogPanel::onLog1Line, Qt::UniqueConnection);
        connect(m_log1, &UsiCommLogModel::engineNameChanged,
                this, &UsiLogPanel::onEngine1NameChanged, Qt::UniqueConnection);
        onEngine1NameChanged();
    }
    if (m_log2) {
        connect(m_log2, &UsiCommLogModel::logLineAppended,
                this, &UsiLogPanel::onLog2Line, Qt::UniqueConnection);
        connect(m_log2, &UsiCommLogModel::engineNameChanged,
                this, &UsiLogPanel::onEngine2NameChanged, Qt::UniqueConnection);
        onEngine2NameChanged();
    }
}

void UsiLogPanel::appendStatus(const QString& message)
{
    if (message.isEmpty()) return;
    appendLine(QStringLiteral("⚙ ") + message, QColor(0x66, 0x66, 0x66));
}

void UsiLogPanel::sendToTarget(const QString& command)
{
    emit usiCommandRequested(m_targetCombo->currentData().toInt(), command);
}

void UsiLogPanel::onEngine1NameChanged()
{
    if (m_log1) {
        QString name = m_log1->engineName();
        if (name.isEmpty()) name = QStringLiteral("---");
        m_engine1Label->setFullText(QStringLiteral("E1: %1").arg(name));
    }
}

void UsiLogPanel::onEngine2NameChanged()
{
    if (m_log2) {
        QString name = m_log2->engineName();
        if (name.isEmpty()) name = QStringLiteral("---");
        m_engine2Label->setFullText(QStringLiteral("E2: %1").arg(name));
    }
}

void UsiLogPanel::onLog1Line(const QString& line)
{
    appendLine(line, kEngine1Color);
}

void UsiLogPanel::onLog2Line(const QString& line)
{
    appendLine(line, kEngine2Color);
}

void UsiLogPanel::onTargetChanged(int index)
{
    AnalysisSettings::usiLogCommandTarget.set(index);
}
