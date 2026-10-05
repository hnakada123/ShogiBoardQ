/// @file tsumeshogigeneratordialog_ui.cpp
/// @brief 詰将棋局面生成ダイアログのレイアウト

#include "tsumeshogigeneratordialog.h"
#include "buttonstyles.h"
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

#include <utility>

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

/// 値が 1 のときは単数形の単位を付ける（英語の「1 piece」と「2 pieces」）。
/// %n の複数形は翻訳ファイルの検査で扱えないため、単数形を別の訳として持つ。
class CountSpinBox : public QSpinBox
{
public:
    CountSpinBox(QString one, QString many, QWidget* parent)
        : QSpinBox(parent)
        , m_one(std::move(one))
        , m_many(std::move(many))
    {
        connect(this, &QSpinBox::valueChanged, this, &CountSpinBox::updateSuffix);
        updateSuffix(value());
    }

private:
    void updateSuffix(int value) { setSuffix(value == 1 ? m_one : m_many); }

    QString m_one;
    QString m_many;
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
    m_settingsScroll = new FittingScrollArea(this);
    m_settingsScroll->setObjectName(QStringLiteral("generatorSettingsScroll"));
    m_settingsScroll->setWidgetResizable(true);
    m_settingsScroll->setFrameShape(QFrame::NoFrame);
    m_settingsScroll->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
    auto* form = new QWidget(m_settingsScroll);
    auto* formLayout = new QVBoxLayout(form);
    formLayout->setContentsMargins(0, 0, 0, 0);
    buildFormSection(formLayout);
    m_settingsScroll->setWidget(form);
    m_settingsScroll->setMinimumHeight(180);
    mainLayout->addWidget(m_settingsScroll);
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
    m_comboEngine = new QComboBox(this);
    m_comboEngine->setObjectName(QStringLiteral("generatorEngine"));
    m_comboEngine->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_comboEngine->setMinimumContentsLength(20);
    engineLabel->setBuddy(m_comboEngine);
    engineRow->addWidget(m_comboEngine, 1);
    m_btnEngineSetting = new QPushButton(tr("設定..."), this);
    engineRow->addWidget(m_btnEngineSetting);
    engineLayout->addLayout(engineRow);
    auto* engineNote = new QLabel(tr("最短手順を返す詰将棋エンジンを使用してください。"), this);
    engineNote->setWordWrap(true);
    auto* hintRow = new QHBoxLayout;
    hintRow->addWidget(engineNote, 1);
    m_btnHelp = new QToolButton(this);
    m_btnHelp->setObjectName(QStringLiteral("generatorHelp"));
    m_btnHelp->setText(tr("エンジン設定・採択条件について"));
    m_btnHelp->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_btnHelp->setCheckable(true);
    m_btnHelp->setArrowType(Qt::RightArrow);
    hintRow->addWidget(m_btnHelp);
    engineLayout->addLayout(hintRow);
    m_labelHelp = new QLabel(
        tr("※ エンジンは最短手順を返す設定で使用してください（KomoringHeights の場合: PostSearchLevel = MinLength）")
        + QStringLiteral("\n")
        + tr("主手順の攻手が一意な局面だけを出力します（成・不成も区別）。玉方が早く詰む変化での別の詰め方は許容し、判定不能の局面は出力しません。"), this);
    m_labelHelp->setWordWrap(true);
    m_labelHelp->hide();
    engineLayout->addWidget(m_labelHelp);
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

    m_spinTargetMoves = new OddSpinBox(this);
    m_spinTargetMoves->setObjectName(QStringLiteral("generatorTargetMoves"));
    m_spinTargetMoves->setRange(1, 99);
    m_spinTargetMoves->setSingleStep(2);
    m_spinTargetMoves->setSuffix(tr(" 手詰"));
    leftForm->addRow(tr("目標手数:"), m_spinTargetMoves);
    m_spinMaxAttack = new CountSpinBox(tr(" 枚", "1 のとき"), tr(" 枚"), this);
    m_spinMaxAttack->setRange(1, 10);
    m_spinMaxAttack->setToolTip(tr("攻方の盤上の駒と持駒の合計枚数の上限です。"));
    leftForm->addRow(tr("攻め駒上限:"), m_spinMaxAttack);
    m_spinMaxDefend = new CountSpinBox(tr(" 枚", "1 のとき"), tr(" 枚"), this);
    m_spinMaxDefend->setRange(0, 5);
    m_spinMaxDefend->setToolTip(tr("玉を除く、玉方の盤上の駒の枚数の上限です。"));
    leftForm->addRow(tr("守り駒上限:"), m_spinMaxDefend);
    m_spinMaxPositions = new CountSpinBox(tr(" 局面", "1 のとき"), tr(" 局面"), this);
    m_spinMaxPositions->setObjectName(QStringLiteral("generatorMaxPositions"));
    m_spinMaxPositions->setRange(0, 10000);
    m_spinMaxPositions->setSpecialValueText(tr("無制限"));
    m_spinMaxPositions->setToolTip(tr("採択した局面がこの件数に達すると終了します。0 は停止するまで生成を続けます。"));
    rightForm->addRow(tr("生成上限:"), m_spinMaxPositions);
    m_spinTimeout = new QSpinBox(this);
    m_spinTimeout->setRange(1, 300);
    m_spinTimeout->setSuffix(tr(" 秒"));
    m_spinTimeout->setToolTip(tr("候補・駒除去後の詰み探索と、それぞれの余詰検査全体に使う時間です。検査時間を超えた局面は採択しません。"));
    rightForm->addRow(tr("探索時間/局面:"), m_spinTimeout);
    m_spinAttackRange = new CountSpinBox(tr(" マス（玉中心）", "1 のとき"), tr(" マス（玉中心）"), this);
    m_spinAttackRange->setRange(1, 8);
    rightForm->addRow(tr("配置範囲:"), m_spinAttackRange);
    auto* options = new QHBoxLayout;
    m_checkAllowFinalAlternatives = new QCheckBox(tr("最終手の複数解を許容する"), this);
    m_checkAllowFinalAlternatives->setToolTip(
        tr("オンにすると、主手順の最終手に複数の詰手があっても採択します（1手詰の初手は除く）。オフにすると最終手も一意な局面だけを出力します。"));
    options->addWidget(m_checkAllowFinalAlternatives);
    options->addStretch();
    m_btnRestoreDefaults = new QPushButton(tr("既定値に戻す"), this);
    m_btnRestoreDefaults->setObjectName(QStringLiteral("generatorRestoreDefaults"));
    m_btnRestoreDefaults->setToolTip(tr("生成設定を既定値に戻します。"));
    options->addWidget(m_btnRestoreDefaults);
    settingsLayout->addLayout(options);
    mainLayout->addWidget(settingsGroup);
}

void TsumeshogiGeneratorDialog::buildProgressSection(QVBoxLayout* mainLayout)
{
    auto* controlLayout = new QHBoxLayout;
    m_btnStart = new QPushButton(tr("開始"), this);
    m_btnStart->setObjectName(QStringLiteral("generatorStart"));
    m_btnStart->setToolTip(tr("現在の結果をクリアして、新しく生成を開始します。"));
    m_btnStart->setStyleSheet(ButtonStyles::primaryAction());
    m_btnStop = new QPushButton(tr("停止"), this);
    m_btnStop->setObjectName(QStringLiteral("generatorStop"));
    m_btnStop->setToolTip(tr("生成を停止します。採択済みの局面は保存・コピーできます。"));
    m_btnStop->setStyleSheet(ButtonStyles::dangerStop());
    controlLayout->addWidget(m_btnStart);
    controlLayout->addWidget(m_btnStop);
    m_labelStatus = new QLabel(this);
    m_labelStatus->setObjectName(QStringLiteral("generatorStatus"));
    m_labelStatus->setTextFormat(Qt::PlainText);
    m_labelStatus->setWordWrap(true);
    controlLayout->addWidget(m_labelStatus, 1);
    mainLayout->addLayout(controlLayout);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setObjectName(QStringLiteral("generatorProgress"));
    m_progressBar->setAccessibleName(tr("生成上限に対する採択局面数"));
    mainLayout->addWidget(m_progressBar);
    auto* progressLayout = new QHBoxLayout;
    m_labelProgress = new QLabel(this);
    m_labelElapsed = new QLabel(this);
    onProgressUpdated(0, 0, 0);
    progressLayout->addWidget(m_labelProgress, 1);
    progressLayout->addWidget(m_labelElapsed);
    mainLayout->addLayout(progressLayout);
    m_labelVerification = new QLabel(this);
    m_labelVerification->setWordWrap(true);
    onVerificationStatsUpdated(0, 0);
    mainLayout->addWidget(m_labelVerification);
}

void TsumeshogiGeneratorDialog::buildResultsSection(QVBoxLayout* mainLayout)
{
    auto* resultLabel = new QLabel(tr("結果一覧"), this);
    resultLabel->setStyleSheet(QStringLiteral("font-weight: bold;"));
    mainLayout->addWidget(resultLabel);

    m_tableResults = new QTableWidget(0, 4, this);
    m_tableResults->setObjectName(QStringLiteral("generatorResults"));
    m_tableResults->setAccessibleName(tr("生成した詰将棋局面"));
    m_tableResults->setHorizontalHeaderLabels({tr("#"), tr("SFEN"), tr("盤面"), tr("手数")});
    m_tableResults->horizontalHeader()->setStretchLastSection(false);
    m_tableResults->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_tableResults->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_tableResults->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_tableResults->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_tableResults->setItemDelegateForColumn(2, new PvBoardButtonDelegate(m_tableResults));
    m_tableResults->verticalHeader()->setVisible(false);
    m_tableResults->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableResults->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_tableResults->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tableResults->setWordWrap(false);
    m_tableResults->setShowGrid(false);
    m_tableResults->setMinimumHeight(180);
    // 空でも表見出しを残し、生成の始め方を結果領域内に案内する。
    auto* emptyLayout = new QVBoxLayout(m_tableResults->viewport());
    m_labelEmptyResults = new QLabel(tr("生成した局面がここに表示されます。\n設定を確認して「開始」を押してください。"), m_tableResults->viewport());
    m_labelEmptyResults->setAlignment(Qt::AlignCenter);
    m_labelEmptyResults->setWordWrap(true);
    m_labelEmptyResults->setAttribute(Qt::WA_TransparentForMouseEvents);
    emptyLayout->addWidget(m_labelEmptyResults);
    mainLayout->addWidget(m_tableResults, 1);

    auto* exportLayout = new QHBoxLayout;
    m_checkIncludePv = new QCheckBox(tr("手順も出力"), this);
    m_checkIncludePv->setObjectName(QStringLiteral("generatorIncludePv"));
    m_checkIncludePv->setToolTip(tr("ファイル保存・コピー時に、SFEN の後ろに USI 形式の詰み手順（moves ...）を付加します"));
    exportLayout->addWidget(m_checkIncludePv);
    exportLayout->addStretch();
    m_btnCopySelected = new QPushButton(tr("選択コピー"), this);
    m_btnCopySelected->setObjectName(QStringLiteral("generatorCopySelected"));
    m_btnCopySelected->setToolTip(tr("選択した局面をコピー（Ctrl+C）。Ctrl・Shift キーで複数選択できます。"));
    exportLayout->addWidget(m_btnCopySelected);
    m_btnCopyAll = new QPushButton(tr("全コピー"), this);
    m_btnCopyAll->setObjectName(QStringLiteral("generatorCopyAll"));
    exportLayout->addWidget(m_btnCopyAll);
    m_btnSaveToFile = new QPushButton(tr("ファイル保存"), this);
    m_btnSaveToFile->setObjectName(QStringLiteral("generatorSave"));
    m_btnSaveToFile->setStyleSheet(ButtonStyles::fileOperation());
    exportLayout->addWidget(m_btnSaveToFile);
    mainLayout->addLayout(exportLayout);
    auto* copy = new QShortcut(QKeySequence::Copy, m_tableResults);
    copy->setContext(Qt::WidgetWithChildrenShortcut);
    connect(copy, &QShortcut::activated, this, &TsumeshogiGeneratorDialog::onCopySelected);

    auto* bottomLayout = new QHBoxLayout;
    m_btnFontDecrease = new QToolButton(this);
    m_btnFontDecrease->setText(QStringLiteral("A-"));
    m_btnFontDecrease->setToolTip(tr("文字サイズを縮小"));
    m_btnFontDecrease->setStyleSheet(ButtonStyles::fontButton());
    bottomLayout->addWidget(m_btnFontDecrease);
    m_btnFontIncrease = new QToolButton(this);
    m_btnFontIncrease->setText(QStringLiteral("A+"));
    m_btnFontIncrease->setToolTip(tr("文字サイズを拡大"));
    m_btnFontIncrease->setStyleSheet(ButtonStyles::fontButton());
    bottomLayout->addWidget(m_btnFontIncrease);
    bottomLayout->addStretch();
    m_btnClose = new QPushButton(tr("閉じる"), this);
    bottomLayout->addWidget(m_btnClose);
    mainLayout->addLayout(bottomLayout);
}
