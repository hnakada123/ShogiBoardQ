/// @file tsumeshogigeneratordialog.cpp
/// @brief 詰将棋局面生成ダイアログクラスの実装

#include "tsumeshogigeneratordialog.h"
#include "tsumeshogikanjibuilder.h"
#include "buttonstyles.h"
#include "changeenginesettingsdialog.h"
#include "tsumeshogisettings.h"
#include "dialogutils.h"
#include "tsumeshogigenerator.h"
#include "pvboarddialog.h"
#include "enginelistsettings.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QSpinBox>
#include <QStyledItemDelegate>
#include <QTableWidget>
#include <QTextStream>
#include <QToolButton>
#include <QVBoxLayout>

/// 「表示」ボタン列用のデリゲート
/// 選択状態に関わらず青背景・白文字を維持する
class BoardButtonDelegate : public QStyledItemDelegate
{
public:
    explicit BoardButtonDelegate(QObject* parent = nullptr) : QStyledItemDelegate(parent) {}

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& /*index*/) const override
    {
        painter->save();

        // 背景色（常に青色）
        QColor bgColor(0x20, 0x9c, 0xee);
        painter->fillRect(option.rect, bgColor);

        // テキスト「表示」を白色で中央に描画
        painter->setPen(Qt::white);
        painter->drawText(option.rect, Qt::AlignCenter, QObject::tr("表示"));

        painter->restore();
    }
};

TsumeshogiGeneratorDialog::TsumeshogiGeneratorDialog(QWidget* parent)
    : QDialog(parent)
    , m_fontHelper({TsumeshogiSettings::tsumeshogiGeneratorFontSize(), 8, 24, 1,
                    TsumeshogiSettings::setTsumeshogiGeneratorFontSize})
{
    setWindowTitle(tr("詰将棋局面生成"));
    setupUi();
    applyFontSize();
    readEngineNameAndDir();
    loadSettings();

    // 初期状態
    setRunningState(false);
}

TsumeshogiGeneratorDialog::~TsumeshogiGeneratorDialog()
{
    if (m_generator && m_generator->isRunning()) {
        m_generator->stop();
    }
    saveSettings();
}

void TsumeshogiGeneratorDialog::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    buildFormSection(mainLayout);
    buildResultsSection(mainLayout);
    connectDialogSignals();
}

void TsumeshogiGeneratorDialog::buildFormSection(QVBoxLayout* mainLayout)
{
    // --- エンジン設定セクション ---
    auto* engineLabel = new QLabel(tr("エンジン設定"), this);
    engineLabel->setStyleSheet(QStringLiteral("font-weight: bold;"));
    mainLayout->addWidget(engineLabel);

    auto* engineLayout = new QHBoxLayout;
    engineLayout->addWidget(new QLabel(tr("エンジン:"), this));
    m_comboEngine = new QComboBox(this);
    m_comboEngine->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    engineLayout->addWidget(m_comboEngine);
    m_btnEngineSetting = new QPushButton(tr("設定..."), this);
    engineLayout->addWidget(m_btnEngineSetting);
    mainLayout->addLayout(engineLayout);

    // --- 生成設定セクション ---
    auto* settingsLabel = new QLabel(tr("生成設定"), this);
    settingsLabel->setStyleSheet(QStringLiteral("font-weight: bold;"));
    mainLayout->addWidget(settingsLabel);

    auto* formLayout = new QFormLayout;

    m_spinTargetMoves = new QSpinBox(this);
    m_spinTargetMoves->setRange(1, 99);
    m_spinTargetMoves->setSingleStep(2);
    m_spinTargetMoves->setValue(3);
    m_spinTargetMoves->setSuffix(tr(" 手詰"));
    formLayout->addRow(tr("目標手数:"), m_spinTargetMoves);
    m_spinMaxAttack = new QSpinBox(this);
    m_spinMaxAttack->setRange(1, 10);
    m_spinMaxAttack->setValue(4);
    m_spinMaxAttack->setSuffix(tr(" 枚"));
    formLayout->addRow(tr("攻め駒上限:"), m_spinMaxAttack);
    m_spinMaxDefend = new QSpinBox(this);
    m_spinMaxDefend->setRange(0, 5);
    m_spinMaxDefend->setValue(1);
    m_spinMaxDefend->setSuffix(tr(" 枚"));
    formLayout->addRow(tr("守り駒上限:"), m_spinMaxDefend);
    m_spinAttackRange = new QSpinBox(this);
    m_spinAttackRange->setRange(1, 8);
    m_spinAttackRange->setValue(3);
    m_spinAttackRange->setSuffix(tr(" マス（玉中心）"));
    formLayout->addRow(tr("配置範囲:"), m_spinAttackRange);
    m_spinTimeout = new QSpinBox(this);
    m_spinTimeout->setRange(1, 300);
    m_spinTimeout->setValue(5);
    m_spinTimeout->setSuffix(tr(" 秒"));
    formLayout->addRow(tr("探索時間/局面:"), m_spinTimeout);
    m_spinMaxPositions = new QSpinBox(this);
    m_spinMaxPositions->setRange(0, 10000);
    m_spinMaxPositions->setValue(10);
    m_spinMaxPositions->setSpecialValueText(tr("無制限"));
    formLayout->addRow(tr("生成上限:"), m_spinMaxPositions);
    mainLayout->addLayout(formLayout);

    // --- 制御ボタン ---
    auto* controlLayout = new QHBoxLayout;
    m_btnStart = new QPushButton(tr("開始"), this);
    m_btnStart->setStyleSheet(ButtonStyles::primaryAction());
    m_btnStop = new QPushButton(tr("停止"), this);
    m_btnStop->setStyleSheet(ButtonStyles::dangerStop());
    controlLayout->addWidget(m_btnStart);
    controlLayout->addWidget(m_btnStop);
    controlLayout->addStretch();
    mainLayout->addLayout(controlLayout);

    // --- プログレス表示 ---
    m_labelProgress = new QLabel(tr("探索済み: 0 局面 / 発見: 0 局面"), this);
    mainLayout->addWidget(m_labelProgress);
    m_labelElapsed = new QLabel(tr("経過時間: 00:00:00"), this);
    mainLayout->addWidget(m_labelElapsed);
}

void TsumeshogiGeneratorDialog::buildResultsSection(QVBoxLayout* mainLayout)
{
    // --- 結果テーブル ---
    auto* resultLabel = new QLabel(tr("結果一覧"), this);
    resultLabel->setStyleSheet(QStringLiteral("font-weight: bold;"));
    mainLayout->addWidget(resultLabel);

    m_tableResults = new QTableWidget(0, 4, this);
    m_tableResults->setHorizontalHeaderLabels({tr("#"), tr("SFEN"), tr("盤面"), tr("手数")});
    m_tableResults->horizontalHeader()->setStretchLastSection(false);
    m_tableResults->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_tableResults->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_tableResults->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_tableResults->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_tableResults->setItemDelegateForColumn(2, new BoardButtonDelegate(m_tableResults));
    m_tableResults->verticalHeader()->setVisible(false);
    m_tableResults->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableResults->setEditTriggers(QAbstractItemView::NoEditTriggers);
    applyTableHeaderStyle();
    mainLayout->addWidget(m_tableResults);

    // --- 下部ボタン行 ---
    auto* bottomLayout = new QHBoxLayout;

    m_btnFontDecrease = new QToolButton(this);
    m_btnFontDecrease->setText(QStringLiteral("A-"));
    m_btnFontDecrease->setStyleSheet(ButtonStyles::fontButton());
    bottomLayout->addWidget(m_btnFontDecrease);
    m_btnFontIncrease = new QToolButton(this);
    m_btnFontIncrease->setText(QStringLiteral("A+"));
    m_btnFontIncrease->setStyleSheet(ButtonStyles::fontButton());
    bottomLayout->addWidget(m_btnFontIncrease);
    m_btnRestoreDefaults = new QPushButton(tr("既定値に戻す"), this);
    m_btnRestoreDefaults->setStyleSheet(ButtonStyles::undoRedo());
    bottomLayout->addWidget(m_btnRestoreDefaults);
    bottomLayout->addStretch();
    m_btnSaveToFile = new QPushButton(tr("ファイル保存"), this);
    m_btnSaveToFile->setStyleSheet(ButtonStyles::fileOperation());
    bottomLayout->addWidget(m_btnSaveToFile);
    m_btnCopySelected = new QPushButton(tr("選択コピー"), this);
    m_btnCopySelected->setStyleSheet(ButtonStyles::editOperation());
    bottomLayout->addWidget(m_btnCopySelected);
    m_btnCopyAll = new QPushButton(tr("全コピー"), this);
    m_btnCopyAll->setStyleSheet(ButtonStyles::editOperation());
    bottomLayout->addWidget(m_btnCopyAll);
    m_btnClose = new QPushButton(tr("閉じる"), this);
    m_btnClose->setStyleSheet(ButtonStyles::secondaryNeutral());
    bottomLayout->addWidget(m_btnClose);

    mainLayout->addLayout(bottomLayout);
}

void TsumeshogiGeneratorDialog::connectDialogSignals()
{
    connect(m_btnStart, &QPushButton::clicked, this, &TsumeshogiGeneratorDialog::onStartClicked);
    connect(m_btnStop, &QPushButton::clicked, this, &TsumeshogiGeneratorDialog::onStopClicked);
    connect(m_btnSaveToFile, &QPushButton::clicked, this, &TsumeshogiGeneratorDialog::onSaveToFile);
    connect(m_btnCopySelected, &QPushButton::clicked, this, &TsumeshogiGeneratorDialog::onCopySelected);
    connect(m_btnCopyAll, &QPushButton::clicked, this, &TsumeshogiGeneratorDialog::onCopyAll);
    connect(m_btnClose, &QPushButton::clicked, this, &TsumeshogiGeneratorDialog::close);
    connect(m_btnFontIncrease, &QToolButton::clicked, this, &TsumeshogiGeneratorDialog::onFontIncrease);
    connect(m_btnFontDecrease, &QToolButton::clicked, this, &TsumeshogiGeneratorDialog::onFontDecrease);
    connect(m_btnRestoreDefaults, &QPushButton::clicked, this, &TsumeshogiGeneratorDialog::onRestoreDefaults);
    connect(m_btnEngineSetting, &QPushButton::clicked, this, &TsumeshogiGeneratorDialog::showEngineSettingsDialog);
    connect(m_tableResults, &QTableWidget::clicked, this, &TsumeshogiGeneratorDialog::onResultTableClicked);

    // ウィンドウサイズ復元
    DialogUtils::restoreDialogSize(this, TsumeshogiSettings::tsumeshogiGeneratorDialogSize());
}

void TsumeshogiGeneratorDialog::readEngineNameAndDir()
{
    const QList<EngineListSettings::EngineEntry> engines = EngineListSettings::loadEngines();
    for (const auto& entry : engines) {
        Engine engine;
        engine.name = entry.name;
        engine.path = entry.path;
        engine.author = entry.author;
        m_comboEngine->addItem(engine.name);
        m_engineList.append(engine);
    }
}

void TsumeshogiGeneratorDialog::loadSettings()
{
    int engineIndex = TsumeshogiSettings::tsumeshogiGeneratorEngineIndex();
    if (engineIndex >= 0 && engineIndex < m_comboEngine->count()) {
        m_comboEngine->setCurrentIndex(engineIndex);
    }

    m_spinTargetMoves->setValue(TsumeshogiSettings::tsumeshogiGeneratorTargetMoves());
    m_spinMaxAttack->setValue(TsumeshogiSettings::tsumeshogiGeneratorMaxAttackPieces());
    m_spinMaxDefend->setValue(TsumeshogiSettings::tsumeshogiGeneratorMaxDefendPieces());
    m_spinAttackRange->setValue(TsumeshogiSettings::tsumeshogiGeneratorAttackRange());
    m_spinTimeout->setValue(TsumeshogiSettings::tsumeshogiGeneratorTimeoutSec());
    m_spinMaxPositions->setValue(TsumeshogiSettings::tsumeshogiGeneratorMaxPositions());
}

void TsumeshogiGeneratorDialog::saveSettings()
{
    DialogUtils::saveDialogSize(this, TsumeshogiSettings::setTsumeshogiGeneratorDialogSize);
    TsumeshogiSettings::setTsumeshogiGeneratorFontSize(m_fontHelper.fontSize());
    TsumeshogiSettings::setTsumeshogiGeneratorEngineIndex(m_comboEngine->currentIndex());
    TsumeshogiSettings::setTsumeshogiGeneratorTargetMoves(m_spinTargetMoves->value());
    TsumeshogiSettings::setTsumeshogiGeneratorMaxAttackPieces(m_spinMaxAttack->value());
    TsumeshogiSettings::setTsumeshogiGeneratorMaxDefendPieces(m_spinMaxDefend->value());
    TsumeshogiSettings::setTsumeshogiGeneratorAttackRange(m_spinAttackRange->value());
    TsumeshogiSettings::setTsumeshogiGeneratorTimeoutSec(m_spinTimeout->value());
    TsumeshogiSettings::setTsumeshogiGeneratorMaxPositions(m_spinMaxPositions->value());
}

void TsumeshogiGeneratorDialog::onStartClicked()
{
    const int engineIndex = m_comboEngine->currentIndex();
    if (engineIndex < 0 || engineIndex >= m_engineList.size()) {
        QMessageBox::critical(this, tr("エラー"), tr("将棋エンジンが選択されていません。"));
        return;
    }

    // ジェネレータを作成
    m_generator = std::make_unique<TsumeshogiGenerator>(this);
    connect(m_generator.get(), &TsumeshogiGenerator::positionFound,
            this, &TsumeshogiGeneratorDialog::onPositionFound);
    connect(m_generator.get(), &TsumeshogiGenerator::progressUpdated,
            this, &TsumeshogiGeneratorDialog::onProgressUpdated);
    connect(m_generator.get(), &TsumeshogiGenerator::finished,
            this, &TsumeshogiGeneratorDialog::onGeneratorFinished);
    connect(m_generator.get(), &TsumeshogiGenerator::errorOccurred,
            this, &TsumeshogiGeneratorDialog::onGeneratorError);

    // 設定を構築
    TsumeshogiGenerator::Settings settings;
    settings.enginePath = m_engineList.at(engineIndex).path;
    settings.engineName = m_engineList.at(engineIndex).name;
    settings.targetMoves = m_spinTargetMoves->value();
    settings.timeoutMs = m_spinTimeout->value() * 1000;
    settings.maxPositionsToFind = m_spinMaxPositions->value();
    settings.posGenSettings.maxAttackPieces = m_spinMaxAttack->value();
    settings.posGenSettings.maxDefendPieces = m_spinMaxDefend->value();
    settings.posGenSettings.attackRange = m_spinAttackRange->value();

    // 結果テーブルをクリア
    m_tableResults->setRowCount(0);

    setRunningState(true);
    m_generator->start(settings);
}

void TsumeshogiGeneratorDialog::onStopClicked()
{
    if (m_generator) {
        m_generator->stop();
    }
}
