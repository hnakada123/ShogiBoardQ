/// @file tsumeshogigeneratordialog_ui.cpp
/// @brief 詰将棋局面生成ダイアログのレイアウト

#include "tsumeshogigeneratordialog.h"
#include "buttonstyles.h"
#include "countspinbox.h"
#include "pvboardbuttondelegate.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QShortcut>
#include <QSpinBox>
#include <QTableWidget>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
/// 偶数の直接入力は確定させず、前の値に戻す。
class OddSpinBox : public QSpinBox
{
public:
    using QSpinBox::QSpinBox;

protected:
    QValidator::State validate(QString& input, int& pos) const override
    {
        const QValidator::State state = QSpinBox::validate(input, pos);
        if (state != QValidator::Acceptable) return state;
        return (valueFromText(input) % 2 != 0) ? QValidator::Acceptable : QValidator::Intermediate;
    }
};

/// 内容の高さを希望サイズにする。QScrollArea::sizeHint は内容の大きさを最初に一度だけ記録するため、
/// 説明の開閉で設定欄の高さが変わらず、スクロールしないと設定が見えなくなる。
class FittingScrollArea : public QScrollArea
{
public:
    using QScrollArea::QScrollArea;

    QSize sizeHint() const override
    {
        if (!widget()) return QScrollArea::sizeHint();
        const int frame = 2 * frameWidth();
        return widget()->sizeHint() + QSize(frame, frame);
    }
};
} // namespace

void TsumeshogiGeneratorDialog::setupUi()
{
    setObjectName(QStringLiteral("tsumeshogiGeneratorDialog"));
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(8);

    // 文字を拡大した場合や小さい画面でも、設定をスクロールして操作できる。
    m_settingsUi.scroll = new FittingScrollArea(this);
    m_settingsUi.scroll->setObjectName(QStringLiteral("generatorSettingsScroll"));
    m_settingsUi.scroll->setWidgetResizable(true);
    m_settingsUi.scroll->setFrameShape(QFrame::NoFrame);
    m_settingsUi.scroll->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
    auto* form = new QWidget(m_settingsUi.scroll);
    auto* formLayout = new QVBoxLayout(form);
    formLayout->setContentsMargins(0, 0, 0, 0);
    buildFormSection(formLayout);
    m_settingsUi.scroll->setWidget(form);
    m_settingsUi.scroll->setMinimumHeight(180);
    mainLayout->addWidget(m_settingsUi.scroll);
    buildProgressSection(mainLayout);
    buildResultsSection(mainLayout);
    connectDialogSignals();

    // 設定入力中の Enter で不用意に生成を開始しない。
    const auto buttons = findChildren<QPushButton*>();
    for (auto* button : buttons) button->setAutoDefault(false);
}

void TsumeshogiGeneratorDialog::buildFormSection(QVBoxLayout* mainLayout)
{
    auto* engineGroup = new QGroupBox(tr("エンジン設定"), this);
    auto* engineLayout = new QVBoxLayout(engineGroup);
    auto* engineRow = new QHBoxLayout;
    auto* engineLabel = new QLabel(tr("エンジン:"), this);
    engineRow->addWidget(engineLabel);
    m_settingsUi.engine = new QComboBox(this);
    m_settingsUi.engine->setObjectName(QStringLiteral("generatorEngine"));
    m_settingsUi.engine->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_settingsUi.engine->setMinimumContentsLength(20);
    engineLabel->setBuddy(m_settingsUi.engine);
    engineRow->addWidget(m_settingsUi.engine, 1);
    m_settingsUi.engineSettings = new QPushButton(tr("設定..."), this);
    engineRow->addWidget(m_settingsUi.engineSettings);
    engineLayout->addLayout(engineRow);
    auto* engineNote = new QLabel(tr("最短手順を返す詰将棋エンジンを使用してください。"), this);
    engineNote->setWordWrap(true);
    auto* hintRow = new QHBoxLayout;
    hintRow->addWidget(engineNote, 1);
    m_settingsUi.help = new QToolButton(this);
    m_settingsUi.help->setObjectName(QStringLiteral("generatorHelp"));
    m_settingsUi.help->setText(tr("エンジン設定・採択条件について"));
    m_settingsUi.help->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_settingsUi.help->setCheckable(true);
    m_settingsUi.help->setArrowType(Qt::RightArrow);
    hintRow->addWidget(m_settingsUi.help);
    engineLayout->addLayout(hintRow);
    m_settingsUi.helpText = new QLabel(
        tr("※ エンジンは最短手順を返す設定で使用してください（KomoringHeights の場合: PostSearchLevel = MinLength）")
        + QStringLiteral("\n")
        + tr("主手順の攻手が一意な局面だけを出力します（成・不成も区別）。玉方が早く詰む変化での別の詰め方は許容し、判定不能の局面は出力しません。"), this);
    m_settingsUi.helpText->setWordWrap(true);
    m_settingsUi.helpText->hide();
    engineLayout->addWidget(m_settingsUi.helpText);
    mainLayout->addWidget(engineGroup);

    auto* settingsGroup = new QGroupBox(tr("生成設定"), this);
    auto* settingsLayout = new QVBoxLayout(settingsGroup);
    auto* columns = new QHBoxLayout;
    columns->setSpacing(24);
    auto* leftForm = new QFormLayout;
    auto* rightForm = new QFormLayout;
    leftForm->setVerticalSpacing(6);
    rightForm->setVerticalSpacing(6);
    leftForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    rightForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    columns->addLayout(leftForm, 1);
    columns->addLayout(rightForm, 1);
    settingsLayout->addLayout(columns);

    m_settingsUi.targetMoves = new OddSpinBox(this);
    m_settingsUi.targetMoves->setObjectName(QStringLiteral("generatorTargetMoves"));
    m_settingsUi.targetMoves->setRange(1, 99);
    m_settingsUi.targetMoves->setSingleStep(2);
    m_settingsUi.targetMoves->setSuffix(tr(" 手詰"));
    leftForm->addRow(tr("目標手数:"), m_settingsUi.targetMoves);
    m_settingsUi.maxAttack = new CountSpinBox(tr(" 枚", "1 のとき"), tr(" 枚"), this);
    m_settingsUi.maxAttack->setRange(1, 10);
    m_settingsUi.maxAttack->setToolTip(tr("攻方の盤上の駒と持駒の合計枚数の上限です。"));
    leftForm->addRow(tr("攻め駒上限:"), m_settingsUi.maxAttack);
    m_settingsUi.maxDefend = new CountSpinBox(tr(" 枚", "1 のとき"), tr(" 枚"), this);
    m_settingsUi.maxDefend->setRange(0, 5);
    m_settingsUi.maxDefend->setToolTip(tr("玉を除く、玉方の盤上の駒の枚数の上限です。"));
    leftForm->addRow(tr("守り駒上限:"), m_settingsUi.maxDefend);
    m_settingsUi.maxPositions = new CountSpinBox(tr(" 局面", "1 のとき"), tr(" 局面"), this);
    m_settingsUi.maxPositions->setObjectName(QStringLiteral("generatorMaxPositions"));
    m_settingsUi.maxPositions->setRange(0, 10000);
    m_settingsUi.maxPositions->setSpecialValueText(tr("無制限"));
    m_settingsUi.maxPositions->setToolTip(tr("採択した局面がこの件数に達すると終了します。0 は停止するまで生成を続けます。"));
    rightForm->addRow(tr("生成上限:"), m_settingsUi.maxPositions);
    m_settingsUi.timeout = new QSpinBox(this);
    m_settingsUi.timeout->setRange(1, 300);
    m_settingsUi.timeout->setSuffix(tr(" 秒"));
    m_settingsUi.timeout->setToolTip(tr("候補・駒除去後の詰み探索と、それぞれの余詰検査全体に使う時間です。検査時間を超えた局面は採択しません。"));
    rightForm->addRow(tr("探索時間/局面:"), m_settingsUi.timeout);
    m_settingsUi.attackRange = new CountSpinBox(tr(" マス（玉中心）", "1 のとき"), tr(" マス（玉中心）"), this);
    m_settingsUi.attackRange->setRange(1, 8);
    rightForm->addRow(tr("配置範囲:"), m_settingsUi.attackRange);
    auto* options = new QHBoxLayout;
    m_settingsUi.allowFinalAlternatives = new QCheckBox(tr("最終手の複数解を許容する"), this);
    m_settingsUi.allowFinalAlternatives->setToolTip(
        tr("オンにすると、主手順の最終手に複数の詰手があっても採択します（1手詰の初手は除く）。オフにすると最終手も一意な局面だけを出力します。"));
    options->addWidget(m_settingsUi.allowFinalAlternatives);
    options->addStretch();
    m_settingsUi.restoreDefaults = new QPushButton(tr("既定値に戻す"), this);
    m_settingsUi.restoreDefaults->setObjectName(QStringLiteral("generatorRestoreDefaults"));
    m_settingsUi.restoreDefaults->setToolTip(tr("生成設定を既定値に戻します。"));
    options->addWidget(m_settingsUi.restoreDefaults);
    settingsLayout->addLayout(options);
    mainLayout->addWidget(settingsGroup);
}

void TsumeshogiGeneratorDialog::buildProgressSection(QVBoxLayout* mainLayout)
{
    auto* controlLayout = new QHBoxLayout;
    m_progressUi.start = new QPushButton(tr("開始"), this);
    m_progressUi.start->setObjectName(QStringLiteral("generatorStart"));
    m_progressUi.start->setToolTip(tr("現在の結果をクリアして、新しく生成を開始します。"));
    m_progressUi.start->setStyleSheet(ButtonStyles::primaryAction());
    m_progressUi.stop = new QPushButton(tr("停止"), this);
    m_progressUi.stop->setObjectName(QStringLiteral("generatorStop"));
    m_progressUi.stop->setToolTip(tr("生成を停止します。採択済みの局面は保存・コピーできます。"));
    m_progressUi.stop->setStyleSheet(ButtonStyles::dangerStop());
    controlLayout->addWidget(m_progressUi.start);
    controlLayout->addWidget(m_progressUi.stop);
    m_progressUi.status = new QLabel(this);
    m_progressUi.status->setObjectName(QStringLiteral("generatorStatus"));
    m_progressUi.status->setTextFormat(Qt::PlainText);
    m_progressUi.status->setWordWrap(true);
    controlLayout->addWidget(m_progressUi.status, 1);
    mainLayout->addLayout(controlLayout);

    m_progressUi.bar = new QProgressBar(this);
    m_progressUi.bar->setObjectName(QStringLiteral("generatorProgress"));
    m_progressUi.bar->setAccessibleName(tr("生成上限に対する採択局面数"));
    mainLayout->addWidget(m_progressUi.bar);
    auto* progressLayout = new QHBoxLayout;
    m_progressUi.counts = new QLabel(this);
    m_progressUi.elapsed = new QLabel(this);
    onProgressUpdated(0, 0, 0);
    progressLayout->addWidget(m_progressUi.counts, 1);
    progressLayout->addWidget(m_progressUi.elapsed);
    mainLayout->addLayout(progressLayout);
    m_progressUi.verification = new QLabel(this);
    m_progressUi.verification->setWordWrap(true);
    onVerificationStatsUpdated(0, 0);
    mainLayout->addWidget(m_progressUi.verification);
}

void TsumeshogiGeneratorDialog::buildResultsSection(QVBoxLayout* mainLayout)
{
    auto* resultLabel = new QLabel(tr("結果一覧"), this);
    resultLabel->setStyleSheet(QStringLiteral("font-weight: bold;"));
    mainLayout->addWidget(resultLabel);

    m_resultsUi.table = new QTableWidget(0, 4, this);
    m_resultsUi.table->setObjectName(QStringLiteral("generatorResults"));
    m_resultsUi.table->setAccessibleName(tr("生成した詰将棋局面"));
    m_resultsUi.table->setHorizontalHeaderLabels({tr("#"), tr("SFEN"), tr("盤面"), tr("手数")});
    m_resultsUi.table->horizontalHeader()->setStretchLastSection(false);
    m_resultsUi.table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_resultsUi.table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_resultsUi.table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_resultsUi.table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_resultsUi.table->setItemDelegateForColumn(2, new PvBoardButtonDelegate(m_resultsUi.table));
    m_resultsUi.table->verticalHeader()->setVisible(false);
    m_resultsUi.table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_resultsUi.table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_resultsUi.table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_resultsUi.table->setWordWrap(false);
    m_resultsUi.table->setShowGrid(false);
    m_resultsUi.table->setMinimumHeight(180);
    // 空でも表見出しを残し、生成の始め方を結果領域内に案内する。
    auto* emptyLayout = new QVBoxLayout(m_resultsUi.table->viewport());
    m_resultsUi.emptyText = new QLabel(tr("生成した局面がここに表示されます。\n設定を確認して「開始」を押してください。"), m_resultsUi.table->viewport());
    m_resultsUi.emptyText->setAlignment(Qt::AlignCenter);
    m_resultsUi.emptyText->setWordWrap(true);
    m_resultsUi.emptyText->setAttribute(Qt::WA_TransparentForMouseEvents);
    emptyLayout->addWidget(m_resultsUi.emptyText);
    mainLayout->addWidget(m_resultsUi.table, 1);

    auto* exportLayout = new QHBoxLayout;
    m_resultsUi.includePv = new QCheckBox(tr("手順も出力"), this);
    m_resultsUi.includePv->setObjectName(QStringLiteral("generatorIncludePv"));
    m_resultsUi.includePv->setToolTip(tr("ファイル保存・コピー時に、SFEN の後ろに USI 形式の詰み手順（moves ...）を付加します"));
    exportLayout->addWidget(m_resultsUi.includePv);
    exportLayout->addStretch();
    m_resultsUi.copySelected = new QPushButton(tr("選択コピー"), this);
    m_resultsUi.copySelected->setObjectName(QStringLiteral("generatorCopySelected"));
    m_resultsUi.copySelected->setToolTip(tr("選択した局面をコピー（Ctrl+C）。Ctrl・Shift キーで複数選択できます。"));
    exportLayout->addWidget(m_resultsUi.copySelected);
    m_resultsUi.copyAll = new QPushButton(tr("全コピー"), this);
    m_resultsUi.copyAll->setObjectName(QStringLiteral("generatorCopyAll"));
    exportLayout->addWidget(m_resultsUi.copyAll);
    m_resultsUi.save = new QPushButton(tr("ファイル保存"), this);
    m_resultsUi.save->setObjectName(QStringLiteral("generatorSave"));
    m_resultsUi.save->setStyleSheet(ButtonStyles::fileOperation());
    exportLayout->addWidget(m_resultsUi.save);
    mainLayout->addLayout(exportLayout);
    auto* copy = new QShortcut(QKeySequence::Copy, m_resultsUi.table);
    copy->setContext(Qt::WidgetWithChildrenShortcut);
    connect(copy, &QShortcut::activated, this, &TsumeshogiGeneratorDialog::onCopySelected);

    auto* bottomLayout = new QHBoxLayout;
    m_footerUi.fontDecrease = new QToolButton(this);
    m_footerUi.fontDecrease->setText(QStringLiteral("A-"));
    m_footerUi.fontDecrease->setToolTip(tr("文字サイズを縮小"));
    m_footerUi.fontDecrease->setStyleSheet(ButtonStyles::fontButton());
    bottomLayout->addWidget(m_footerUi.fontDecrease);
    m_footerUi.fontIncrease = new QToolButton(this);
    m_footerUi.fontIncrease->setText(QStringLiteral("A+"));
    m_footerUi.fontIncrease->setToolTip(tr("文字サイズを拡大"));
    m_footerUi.fontIncrease->setStyleSheet(ButtonStyles::fontButton());
    bottomLayout->addWidget(m_footerUi.fontIncrease);
    bottomLayout->addStretch();
    m_footerUi.close = new QPushButton(tr("閉じる"), this);
    bottomLayout->addWidget(m_footerUi.close);
    mainLayout->addLayout(bottomLayout);
}
