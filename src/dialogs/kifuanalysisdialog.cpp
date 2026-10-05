/// @file kifuanalysisdialog.cpp
/// @brief 棋譜解析ダイアログクラスの実装

#include "kifuanalysisdialog.h"
#include "changeenginesettingsdialog.h"
#include "ui_kifuanalysisdialog.h"
#include "analysissettings.h"
#include "dialogutils.h"
#include "enginelistsettings.h"
#include "shogiutils.h"
#include "buttonstyles.h"

#include <QFile>
#include <QComboBox>
#include <QLabel>
#include <QAbstractItemView>
#include <QTextStream>
#include <QTimer>
#include <qmessagebox.h>

// 棋譜解析ダイアログのUIを設定する。
KifuAnalysisDialog::KifuAnalysisDialog(QWidget *parent)
    : QDialog(parent)
    , ui(std::make_unique<Ui::KifuAnalysisDialog>())
    , m_fontHelper({AnalysisSettings::kifuAnalysisFontSize() > 0
                        ? AnalysisSettings::kifuAnalysisFontSize() : qMax(10, font().pointSize()), 8, 24, 1,
                    AnalysisSettings::setKifuAnalysisFontSize})
{
    // UIをセットアップする。
    ui->setupUi(this);

    ui->buttonBox->button(QDialogButtonBox::Ok)->setText(tr("解析開始"));
    ui->buttonBox->button(QDialogButtonBox::Ok)->setStyleSheet(ButtonStyles::primaryAction());
    ui->btnFontDecrease->setStyleSheet(ButtonStyles::panelToolButton());
    ui->btnFontIncrease->setStyleSheet(ButtonStyles::panelToolButton());
    applyRangeLabels();
    ui->spinBoxStartPly->setAccessibleName(tr("解析開始手数"));
    ui->spinBoxEndPly->setAccessibleName(tr("解析終了手数"));
    ui->label->setBuddy(ui->byoyomiSec);
    ui->comboBoxEngine1->setAccessibleName(tr("解析エンジン"));
    ui->comboBoxEngine1->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    ui->comboBoxEngine1->setMinimumContentsLength(16);
    ui->analysisSummary->setStyleSheet(QStringLiteral(
        "background: #e7f0f8; color: #243b53; border-radius: 4px; padding: 10px;"));

    // フォントサイズを適用
    applyFontSize();

    // 設定ファイルからエンジンの名前とディレクトリを読み込む。
    readEngineNameAndDir();
    
    // 設定から前回選択したエンジンを復元
    int savedEngineIndex = AnalysisSettings::kifuAnalysisEngineIndex();
    if (savedEngineIndex >= 0 && savedEngineIndex < ui->comboBoxEngine1->count()) {
        ui->comboBoxEngine1->setCurrentIndex(savedEngineIndex);
    }
    
    // 設定から前回の思考時間を復元
    int savedByoyomi = AnalysisSettings::kifuAnalysisByoyomiSec();
    if (savedByoyomi > 0) {
        ui->byoyomiSec->setValue(savedByoyomi);
    }
    
    // 設定から解析範囲を復元
    bool savedFullRange = AnalysisSettings::kifuAnalysisFullRange();
    if (savedFullRange) {
        ui->radioButtonInitPosition->setChecked(true);
    } else {
        ui->radioButtonRangePosition->setChecked(true);
    }
    
    // 設定から開始・終了手数を復元（setMaxPlyで上書きされる可能性があるが初期値として設定）
    m_savedStartPly = AnalysisSettings::kifuAnalysisStartPly();
    m_savedEndPly = AnalysisSettings::kifuAnalysisEndPly();

    // エンジン設定ボタンが押されたときの処理
    connect(ui->engineSetting, &QPushButton::clicked, this, &KifuAnalysisDialog::showEngineSettingsDialog);

    // 条件を検証・保存してからダイアログを閉じる。
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &KifuAnalysisDialog::processEngineSettings);

    // キャンセルボタンが押された場合、ダイアログを拒否する動作を行う。
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &KifuAnalysisDialog::reject);
    
    // 範囲指定ラジオボタンのトグル処理
    connect(ui->radioButtonRangePosition, &QRadioButton::toggled, this, &KifuAnalysisDialog::onRangeRadioToggled);
    
    // 開始手数が変更された場合、終了手数の最小値を更新
    connect(ui->spinBoxStartPly, QOverload<int>::of(&QSpinBox::valueChanged), this, &KifuAnalysisDialog::onStartPlyChanged);
    connect(ui->spinBoxEndPly, &QSpinBox::valueChanged, this, &KifuAnalysisDialog::updateSummary);
    connect(ui->byoyomiSec, &QSpinBox::valueChanged, this, &KifuAnalysisDialog::updateSummary);
    
    // フォントサイズ調整ボタン
    connect(ui->btnFontIncrease, &QPushButton::clicked, this, &KifuAnalysisDialog::onFontIncrease);
    connect(ui->btnFontDecrease, &QPushButton::clicked, this, &KifuAnalysisDialog::onFontDecrease);
    
    // 範囲指定のスピンボックスの有効/無効を設定
    bool rangeEnabled = ui->radioButtonRangePosition->isChecked();
    ui->spinBoxStartPly->setEnabled(rangeEnabled);
    ui->spinBoxEndPly->setEnabled(rangeEnabled);

    const bool hasEngine = !m_engineList.isEmpty();
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(hasEngine);
    ui->engineSetting->setEnabled(hasEngine);
    ui->engineHint->setVisible(!hasEngine);
    updateSummary();

    // ウィンドウサイズを復元
    DialogUtils::restoreDialogSize(this, AnalysisSettings::kifuAnalysisDialogSize());
}

KifuAnalysisDialog::~KifuAnalysisDialog() = default;

void KifuAnalysisDialog::done(int result)
{
    DialogUtils::saveDialogSize(this, AnalysisSettings::setKifuAnalysisDialogSize);
    QDialog::done(result);
}

void KifuAnalysisDialog::resizeEvent(QResizeEvent* event)
{
    QDialog::resizeEvent(event);
    updateMinimumSize();
}

void KifuAnalysisDialog::updateMinimumSize()
{
    // 折り返す説明・所要時間も、文字拡大時に欠けない高さを確保する。
    const int contentHeight = ui->mainLayout->totalHeightForWidth(width());
    setMinimumHeight(qMax(280, contentHeight));
}

void KifuAnalysisDialog::updateSummary()
{
    const int count = ui->radioButtonInitPosition->isChecked()
        ? m_maxPly + 1 : ui->spinBoxEndPly->value() - ui->spinBoxStartPly->value() + 1;
    const qint64 seconds = static_cast<qint64>(count) * ui->byoyomiSec->value();
    QString duration;
    if (seconds < 60) duration = tr("約%1秒").arg(seconds);
    else if (seconds < 3600) duration = tr("約%1分%2秒").arg(seconds / 60).arg(seconds % 60);
    else duration = tr("約%1時間%2分").arg(seconds / 3600).arg((seconds % 3600) / 60);
    ui->analysisSummary->setText(tr("解析対象: %1局面　所要時間の目安: %2").arg(count).arg(duration));
    ui->analysisSummary->setToolTip(tr("エンジンの起動時間などにより、実際の所要時間は前後します。"));
}

// 範囲指定ラジオボタンが選択された場合
void KifuAnalysisDialog::onRangeRadioToggled(bool checked)
{
    ui->spinBoxStartPly->setEnabled(checked);
    ui->spinBoxEndPly->setEnabled(checked);
    updateSummary();
}

// 開始手数が変更された場合
void KifuAnalysisDialog::onStartPlyChanged(int value)
{
    // 終了手数は開始手数以上でなければならない
    ui->spinBoxEndPly->setMinimum(value);
    if (ui->spinBoxEndPly->value() < value) {
        ui->spinBoxEndPly->setValue(value);
    }
    updateSummary();
}

// 最大手数を設定する
void KifuAnalysisDialog::setMaxPly(int maxPly)
{
    maxPly = qMax(0, maxPly);
    m_maxPly = maxPly;
    
    // スピンボックスの最大値を設定
    ui->spinBoxStartPly->setMaximum(maxPly);
    ui->spinBoxEndPly->setMaximum(maxPly);
    
    // 保存された値を復元（最大値の範囲内に収める）
    int startVal = qBound(0, m_savedStartPly, maxPly);
    int endVal = qBound(startVal, m_savedEndPly, maxPly);
    
    // 未設定の場合は最終局面までを初期値とする（0は開始局面のみ）。
    if (m_savedEndPly < 0) {
        endVal = maxPly;
    }
    
    ui->spinBoxStartPly->setValue(startVal);
    ui->spinBoxEndPly->setValue(endVal);
    updateSummary();
}

// エンジン設定ボタンが押された場合、エンジン設定ダイアログを表示する。
void KifuAnalysisDialog::showEngineSettingsDialog()
{
    // 選択したエンジン番号を取得する。
    m_engineNumber = ui->comboBoxEngine1->currentIndex();

    // 選択したエンジン名を取得する。
    m_engineName = ui->comboBoxEngine1->currentText();

    // エンジン名が空の場合
    if (m_engineName.isEmpty()) {
        // エラーメッセージを表示する。
        const QString errorMessage = tr("将棋エンジンが選択されていません。");

        // エラーメッセージを表示する。
        QMessageBox::critical(this, tr("エラー"), errorMessage);

        return;
    }
    // エンジン名が空でない場合
    else {
        // エンジン設定ダイアログを表示する。
        ChangeEngineSettingsDialog dialog(this);

        // エンジン名を設定する。
        dialog.setEngineName(m_engineName);

        // エンジン設定ダイアログを作成する。
        dialog.setupEngineOptionsDialog();

        // ダイアログがキャンセルされた場合、何もしない。
        if (dialog.exec() == QDialog::Rejected) return;
    }
}

// OKボタンが押された場合、エンジン名、エンジン番号、解析局面フラグ、思考時間を取得する。
void KifuAnalysisDialog::processEngineSettings()
{
    if (ui->comboBoxEngine1->currentIndex() < 0) return;
    // 選択したエンジン名を取得する。
    m_engineName = ui->comboBoxEngine1->currentText();

    // 選択したエンジン番号を取得する。
    m_engineNumber = ui->comboBoxEngine1->currentIndex();

    // "開始局面から最終手まで"にチェックが入っている場合
    if (ui->radioButtonInitPosition->isChecked()) {
        m_initPosition = true;
        m_startPly = 0;
        m_endPly = m_maxPly;
    }
    // 範囲指定にチェックが入っている場合
    else if (ui->radioButtonRangePosition->isChecked()) {
        m_initPosition = false;
        m_startPly = ui->spinBoxStartPly->value();
        m_endPly = ui->spinBoxEndPly->value();
    }

    // 1手あたりの思考時間（秒数）を取得する。
    m_byoyomiSec = ui->byoyomiSec->value();
    
    // 設定を保存
    AnalysisSettings::setKifuAnalysisEngineIndex(m_engineNumber);
    AnalysisSettings::setKifuAnalysisByoyomiSec(m_byoyomiSec);
    AnalysisSettings::setKifuAnalysisFullRange(m_initPosition);
    AnalysisSettings::setKifuAnalysisStartPly(ui->spinBoxStartPly->value());
    AnalysisSettings::setKifuAnalysisEndPly(ui->spinBoxEndPly->value());
    accept();
}

// エンジンの名前とディレクトリを格納するリストを取得する。
QList<KifuAnalysisDialog::Engine> KifuAnalysisDialog::engineList() const
{
    return m_engineList;
}

// エンジン名を取得する。
QString KifuAnalysisDialog::engineName() const
{
    return m_engineName;
}

// エンジン番号を取得する。
int KifuAnalysisDialog::engineNumber() const
{
    return m_engineNumber;
}

// 1手あたりの思考時間（秒数）を取得する。
int KifuAnalysisDialog::byoyomiSec() const
{
    return m_byoyomiSec;
}

// "開始局面から最終手まで"を選択したかどうかのフラグを取得する。
bool KifuAnalysisDialog::initPosition() const
{
    return m_initPosition;
}

// 範囲指定の開始手数を取得する
int KifuAnalysisDialog::startPly() const
{
    return m_startPly;
}

// 範囲指定の終了手数を取得する
int KifuAnalysisDialog::endPly() const
{
    return m_endPly;
}

// 設定ファイルからエンジンの名前とディレクトリを読み込む。
void KifuAnalysisDialog::readEngineNameAndDir()
{
    const QList<EngineListSettings::EngineEntry> engines = EngineListSettings::loadEngines();
    for (const auto& entry : engines) {
        Engine engine;
        engine.name = entry.name;
        engine.path = entry.path;

        // 棋譜解析ダイアログのエンジン選択リストにエンジン名を追加する。
        ui->comboBoxEngine1->addItem(engine.name);

        // エンジンリストにエンジンを追加する。
        m_engineList.append(engine);
    }
}

// フォントサイズ拡大
void KifuAnalysisDialog::onFontIncrease()
{
    if (m_fontHelper.increase()) applyFontSize();
}

// フォントサイズ縮小
void KifuAnalysisDialog::onFontDecrease()
{
    if (m_fontHelper.decrease()) applyFontSize();
}

// 範囲指定の数値欄の前後に置く文字列を設定する。
// 語順は言語ごとに異なるため、1つの文を %1・%2 の位置で分けて数値欄の前・間・後ろに置く。
void KifuAnalysisDialog::applyRangeLabels()
{
    QString pattern = tr("%1手目から%2手目まで");
    qsizetype first = pattern.indexOf(QStringLiteral("%1"));
    qsizetype second = pattern.indexOf(QStringLiteral("%2"));
    if (first < 0 || second < first) {
        pattern = QStringLiteral("%1手目から%2手目まで");
        first = pattern.indexOf(QStringLiteral("%1"));
        second = pattern.indexOf(QStringLiteral("%2"));
    }
    ui->labelRangePrefix->setText(pattern.left(first).trimmed());
    ui->labelFrom->setText(pattern.mid(first + 2, second - first - 2).trimmed());
    ui->labelTo->setText(pattern.mid(second + 2).trimmed());
    ui->labelRangePrefix->setVisible(!ui->labelRangePrefix->text().isEmpty());
    ui->labelTo->setVisible(!ui->labelTo->text().isEmpty());
}

// フォントサイズを適用
void KifuAnalysisDialog::applyFontSize()
{
    DialogUtils::standardizeDialog(this);
    const int size = m_fontHelper.fontSize();
    QFont f = font();
    f.setPointSize(size);
    setFont(f);

    // コンストラクタ中は setFont() による子ウィジェットへのフォント伝播が
    // 遅延するため、全子ウィジェットに明示的にフォントを設定する
    const QList<QWidget*> widgets = findChildren<QWidget*>();
    for (QWidget* widget : std::as_const(widgets)) {
        if (widget) {
            widget->setFont(f);
        }
    }

    // セクション見出しラベルを太字にする
    QFont boldFont = f;
    boldFont.setBold(true);
    ui->labelSectionEngine->setFont(boldFont);
    ui->labelSectionRange->setFont(boldFont);
    ui->labelSectionTime->setFont(boldFont);
    ui->btnFontDecrease->setEnabled(size > 8);
    ui->btnFontIncrease->setEnabled(size < 24);
    const int buttonWidth = qMax(40, fontMetrics().horizontalAdvance(QStringLiteral("A+")) + 20);
    ui->btnFontDecrease->setFixedWidth(buttonWidth);
    ui->btnFontIncrease->setFixedWidth(buttonWidth);

    // コンボボックスのポップアップリストにも反映する
    const QList<QComboBox*> comboBoxes = findChildren<QComboBox*>();
    for (QComboBox* comboBox : std::as_const(comboBoxes)) {
        if (comboBox && comboBox->view()) {
            comboBox->view()->setFont(f);
        }
    }
    ui->mainLayout->invalidate();
    QTimer::singleShot(0, this, &KifuAnalysisDialog::updateMinimumSize);
    DialogUtils::updateFontButtons(this, size);
    DialogUtils::fitWrappedLabels(this);
}
