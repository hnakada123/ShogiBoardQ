/// @file josekimergedialog.cpp
/// @brief 棋譜の手を選んで定跡に登録するダイアログの実装

#include "josekimergedialog.h"

#include "buttonstyles.h"
#include "dialogfontscale.h"
#include "fontsizehelper.h"
#include "josekibook.h"
#include "josekibooksession.h"
#include "josekiimport.h"
#include "josekisettings.h"
#include "kifumovedelegate.h"
#include "kifupresentation.h"

#include <QButtonGroup>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSet>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {
constexpr QSize kMinimumSize{500, 400};
} // namespace

JosekiMergeDialog::JosekiMergeDialog(JosekiBookSession *session, const RecordLine &line, QWidget *parent)
    : QDialog(parent)
    , m_session(session)
    , m_currentPly(line.currentPly)
{
    Q_ASSERT(m_session);
    // 手順の指し手（終局の行は登録しない）。表記は棋譜欄と同じ
    for (qsizetype i = 0; i < line.entries.size(); ++i) {
        const RecordEntry &entry = line.entries.at(i);
        if (entry.isTerminal()) break;
        Row row;
        row.ply = line.startPly + static_cast<int>(i);
        row.sfen = line.sfenAt(static_cast<int>(i));
        row.usiMove = entry.move.move.toString();
        if (row.usiMove.isEmpty()) continue;
        row.moveText = KifuPresentation::entryLabel(entry, row.sfen);
        m_rows.append(row);
    }
    setupUi();
    connect(m_session, &JosekiBookSession::contentChanged, this, &JosekiMergeDialog::updateTable);
    connect(m_session, &JosekiBookSession::busyChanged, this, &JosekiMergeDialog::updateEnabled);
    connect(m_session, &JosekiBookSession::deferredLoadChanged, this, &JosekiMergeDialog::updateEnabled);
    connect(m_session, &JosekiBookSession::filePathChanged, this, &JosekiMergeDialog::updateTargetLabel);
    updateTargetLabel();
    updateTable();
    updateEnabled();
}

JosekiMergeDialog::~JosekiMergeDialog() = default;

void JosekiMergeDialog::setupUi()
{
    setWindowTitle(tr("棋譜から定跡にマージ"));
    setMinimumSize(kMinimumSize);

    auto *mainLayout = new QVBoxLayout(this);

    m_fontDecreaseBtn = new QPushButton(this);
    m_fontIncreaseBtn = new QPushButton(this);
    m_statusLabel = new QLabel(m_rows.isEmpty() ? tr("登録する指し手がありません。") : tr("%1手の棋譜").arg(m_rows.size()), this);
    m_statusLabel->setWordWrap(true);

    // === マージ先ファイル表示 ===
    m_targetFileLabel = new QLabel(this);
    m_targetFileLabel->setObjectName(QStringLiteral("mergeTargetLabel"));
    m_targetFileLabel->setWordWrap(true);
    m_targetFileLabel->setTextFormat(Qt::PlainText);
    m_targetFileLabel->setStyleSheet(QStringLiteral("QLabel { font-weight: bold; }"));
    mainLayout->addWidget(m_targetFileLabel);

    auto *descLabel = new QLabel(tr("棋譜の指し手を定跡に登録します。「登録」ボタンで個別に、「全て登録」で一括登録できます。"), this);
    descLabel->setWordWrap(true);
    mainLayout->addWidget(descLabel);

    auto *saveLabel = new QLabel(tr("※ 登録した手は、定跡ウィンドウの「保存」で定跡ファイルに保存してください"), this);
    saveLabel->setWordWrap(true);
    mainLayout->addWidget(saveLabel);

    // やねうら王は既定（BookDepthLimit=16）では深さ16未満の定跡手を使わない
    auto *engineNoteLabel = new QLabel(tr("※ 登録した手は評価値0・深さ0になります。やねうら王に読ませるときは、"
                                          "エンジン設定で BookDepthLimit を 0 にしてください（既定の16では使われません）"),
                                       this);
    engineNoteLabel->setObjectName(QStringLiteral("engineNoteLabel"));
    engineNoteLabel->setWordWrap(true);
    mainLayout->addWidget(engineNoteLabel);

    // === テーブル ===
    m_tableWidget = new QTableWidget(this);
    m_tableWidget->setItemDelegateForColumn(1, new KifuMoveDelegate(m_tableWidget));
    m_registerButtons = new QButtonGroup(this);
    connect(m_registerButtons, &QButtonGroup::idClicked, this, &JosekiMergeDialog::onRegisterButtonClicked);
    m_tableWidget->setColumnCount(4);
    m_tableWidget->setHorizontalHeaderLabels({tr("手数"), tr("指し手"), tr("登録"), tr("状態")});
    m_tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tableWidget->setAlternatingRowColors(true);
    m_tableWidget->verticalHeader()->setVisible(false);
    m_tableWidget->horizontalHeader()->setStretchLastSection(true);

    mainLayout->addWidget(m_statusLabel);
    mainLayout->addWidget(m_tableWidget, 1);

    // === ボタン行 ===
    auto *buttonLayout = new QHBoxLayout();
    buttonLayout->addWidget(m_fontDecreaseBtn);
    buttonLayout->addWidget(m_fontIncreaseBtn);
    buttonLayout->addStretch();

    m_registerAllButton = new QPushButton(tr("全て登録"), this);
    m_registerAllButton->setObjectName(QStringLiteral("registerAllMoves"));
    m_registerAllButton->setAutoDefault(false);
    m_registerAllButton->setToolTip(tr("全ての指し手を定跡に登録"));
    m_registerAllButton->setStyleSheet(ButtonStyles::primaryAction());
    buttonLayout->addWidget(m_registerAllButton);

    m_closeButton = new QPushButton(tr("閉じる"), this);
    m_closeButton->setStyleSheet(ButtonStyles::secondaryNeutral());
    buttonLayout->addWidget(m_closeButton);
    mainLayout->addLayout(buttonLayout);

    connect(m_registerAllButton, &QPushButton::clicked, this, &JosekiMergeDialog::onRegisterAllButtonClicked);
    connect(m_closeButton, &QPushButton::clicked, this, &QDialog::accept);

    // 文字の大きさとウィンドウの大きさ（どの閉じ方でも保存する）
    DialogFontScale::Options fontOptions;
    fontOptions.maxSize = 20;
    fontOptions.decrease = m_fontDecreaseBtn;
    fontOptions.increase = m_fontIncreaseBtn;
    fontOptions.size = &JosekiSettings::josekiMergeDialogSize;
    const DialogFontScale *scale = DialogFontScale::install(this, JosekiSettings::josekiMergeDialogFontSize, fontOptions);
    connect(scale->fontSize(), &FontSizeHelper::fontSizeChanged, this, &JosekiMergeDialog::fitRowsToFont);
}

bool JosekiMergeDialog::isRegistered(const Row &row) const
{
    return m_session->hasMove(row.sfen, row.usiMove);
}

void JosekiMergeDialog::updateTable()
{
    const int rowCount = static_cast<int>(m_rows.size());
    m_tableWidget->setRowCount(rowCount);
    bool hasUnregistered = false;
    for (int i = 0; i < rowCount; ++i) {
        const Row &row = m_rows.at(i);
        auto *plyItem = new QTableWidgetItem(QString::number(row.ply));
        plyItem->setTextAlignment(Qt::AlignCenter);
        m_tableWidget->setItem(i, 0, plyItem);
        auto *moveItem = new QTableWidgetItem(row.moveText);
        m_tableWidget->setItem(i, 1, moveItem);

        const bool registered = isRegistered(row);
        hasUnregistered = hasUnregistered || !registered;
        auto *registerBtn = new QPushButton(registered ? tr("登録済") : tr("登録"), this);
        registerBtn->setAutoDefault(false);
        registerBtn->setStyleSheet(ButtonStyles::tableRegisterButton());
        registerBtn->setEnabled(!registered);
        if (QAbstractButton *old = m_registerButtons->button(i)) m_registerButtons->removeButton(old);
        m_registerButtons->addButton(registerBtn, i);
        m_tableWidget->setCellWidget(i, 2, registerBtn);

        auto *statusItem = new QTableWidgetItem();
        statusItem->setTextAlignment(Qt::AlignCenter);
        if (registered) {
            statusItem->setText(tr("✓登録済"));
            statusItem->setForeground(QColor(0, 128, 0));
        }
        m_tableWidget->setItem(i, 3, statusItem);

        // 表示中の手の行を目立たせる
        if (row.ply == m_currentPly) {
            const QColor highlight(255, 255, 200);
            for (QTableWidgetItem *item : {plyItem, moveItem, statusItem}) item->setBackground(highlight);
        }
    }
    m_registerAllButton->setEnabled(hasUnregistered);
    // 行を入れてから列幅を合わせる（空の表で合わせると指し手が縦に折り返し、登録ボタンの文字が切れる）
    fitRowsToFont();
}

void JosekiMergeDialog::updateEnabled()
{
    setEnabled(m_session->canEdit());
}

void JosekiMergeDialog::updateTargetLabel()
{
    const QString filePath = m_session->filePath();
    m_targetFileLabel->setText(filePath.isEmpty() ? tr("マージ先: (未設定)")
                                                  : tr("マージ先: %1").arg(QFileInfo(filePath).fileName()));
    m_targetFileLabel->setToolTip(filePath);
}

void JosekiMergeDialog::onRegisterButtonClicked(int index)
{
    if (index < 0 || index >= m_rows.size() || !m_session->canEdit()) return;
    const Row &row = m_rows.at(index);
    if (isRegistered(row)) return;
    // 定跡が変わると updateTable で表を作り直す
    m_session->importMoves({JosekiImport::Move{row.sfen, row.usiMove, 1, {}}});
}

void JosekiMergeDialog::onRegisterAllButtonClicked()
{
    if (!m_session->canEdit()) return;
    QList<JosekiImport::Move> pending;
    QSet<QString> pendingKeys;
    for (const Row &row : std::as_const(m_rows)) {
        const QString key = JosekiBook::positionKey(row.sfen) + QLatin1Char(' ') + row.usiMove;
        if (isRegistered(row) || pendingKeys.contains(key)) continue;
        pendingKeys.insert(key);
        pending.append(JosekiImport::Move{row.sfen, row.usiMove, 1, {}});
    }
    if (pending.isEmpty()) {
        QMessageBox::information(this, tr("情報"), tr("登録する指し手がありません。"));
        return;
    }
    const JosekiBookSession::ImportStats stats = m_session->importMoves(pending);
    QMessageBox::information(this, tr("一括登録完了"), tr("%1件の指し手を定跡に登録しました。").arg(stats.added));
}

void JosekiMergeDialog::fitRowsToFont()
{
    m_tableWidget->verticalHeader()->setDefaultSectionSize(QFontMetrics(m_tableWidget->font()).height() + 16);
    m_tableWidget->resizeRowsToContents();
    m_tableWidget->resizeColumnsToContents();
}
