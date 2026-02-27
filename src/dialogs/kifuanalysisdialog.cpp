/// @file kifuanalysisdialog.cpp
/// @brief 棋譜解析ダイアログクラスの実装

#include "kifuanalysisdialog.h"
#include "changeenginesettingsdialog.h"
#include "ui_kifuanalysisdialog.h"
#include "enginesettingsconstants.h"
#include "settingscommon.h"
#include "analysissettings.h"
#include "dialogutils.h"
#include "shogiutils.h"

#include <QFile>
#include <QComboBox>
#include <QLabel>
#include <QAbstractItemView>
#include <QTextStream>
#include <qmessagebox.h>

using namespace EngineSettingsConstants;

// 棋譜解析ダイアログのUIを設定する。
KifuAnalysisDialog::KifuAnalysisDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::KifuAnalysisDialog)
    , m_fontHelper({AnalysisSettings::kifuAnalysisFontSize(), 8, 24, 2,
                    AnalysisSettings::setKifuAnalysisFontSize})
{
    // UIをセットアップする。
    ui->setupUi(this);

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

    // OKボタンが押された場合、エンジン名、エンジン番号、解析局面フラグ、思考時間を取得する。
    // 重要: accept()より先に接続し、メンバー変数が確実に更新されるようにする
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &KifuAnalysisDialog::processEngineSettings);

    // OKボタンが押された場合、ダイアログを受け入れたとして閉じる動作を行う。
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &KifuAnalysisDialog::accept);

    // キャンセルボタンが押された場合、ダイアログを拒否する動作を行う。
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &KifuAnalysisDialog::reject);
    
    // 範囲指定ラジオボタンのトグル処理
    connect(ui->radioButtonRangePosition, &QRadioButton::toggled, this, &KifuAnalysisDialog::onRangeRadioToggled);
    
    // 開始手数が変更された場合、終了手数の最小値を更新
    connect(ui->spinBoxStartPly, QOverload<int>::of(&QSpinBox::valueChanged), this, &KifuAnalysisDialog::onStartPlyChanged);
    
    // フォントサイズ調整ボタン
    connect(ui->btnFontIncrease, &QPushButton::clicked, this, &KifuAnalysisDialog::onFontIncrease);
    connect(ui->btnFontDecrease, &QPushButton::clicked, this, &KifuAnalysisDialog::onFontDecrease);
    
    // 範囲指定のスピンボックスの有効/無効を設定
    bool rangeEnabled = ui->radioButtonRangePosition->isChecked();
    ui->spinBoxStartPly->setEnabled(rangeEnabled);
    ui->spinBoxEndPly->setEnabled(rangeEnabled);

    // ウィンドウサイズを復元
    DialogUtils::restoreDialogSize(this, AnalysisSettings::kifuAnalysisDialogSize());
}

// 範囲指定ラジオボタンが選択された場合
void KifuAnalysisDialog::onRangeRadioToggled(bool checked)
{
    ui->spinBoxStartPly->setEnabled(checked);
    ui->spinBoxEndPly->setEnabled(checked);
}

// 開始手数が変更された場合
void KifuAnalysisDialog::onStartPlyChanged(int value)
{
    // 終了手数は開始手数以上でなければならない
    ui->spinBoxEndPly->setMinimum(value);
    if (ui->spinBoxEndPly->value() < value) {
        ui->spinBoxEndPly->setValue(value);
    }
}

// 最大手数を設定する
void KifuAnalysisDialog::setMaxPly(int maxPly)
{
    m_maxPly = maxPly;
    
    // スピンボックスの最大値を設定
    ui->spinBoxStartPly->setMaximum(maxPly);
    ui->spinBoxEndPly->setMaximum(maxPly);
    
    // 保存された値を復元（最大値の範囲内に収める）
    int startVal = qBound(0, m_savedStartPly, maxPly);
    int endVal = qBound(startVal, m_savedEndPly, maxPly);
    
    // 保存値が0の場合はデフォルト値を使用
    if (m_savedEndPly == 0) {
        endVal = maxPly;
    }
    
    ui->spinBoxStartPly->setValue(startVal);
    ui->spinBoxEndPly->setValue(endVal);
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
        const QString errorMessage = tr("An error occurred in KifuAnalysisDialog::showEngineSettingsDialog. The Shogi engine has not been selected.");

        // エラーメッセージを表示する。
        QMessageBox::critical(this, tr("Error"), errorMessage);

        return;
    }
    // エンジン名が空でない場合
    else {
        // エンジン設定ダイアログを表示する。
        ChangeEngineSettingsDialog dialog(this);

        // エンジン名、エンジン番号を設定する。
        dialog.setEngineNumber(m_engineNumber);
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
    m_byoyomiSec = ui->byoyomiSec->text().toInt();
    
    // 設定を保存
    AnalysisSettings::setKifuAnalysisEngineIndex(m_engineNumber);
    AnalysisSettings::setKifuAnalysisByoyomiSec(m_byoyomiSec);
    AnalysisSettings::setKifuAnalysisFullRange(m_initPosition);
    AnalysisSettings::setKifuAnalysisStartPly(ui->spinBoxStartPly->value());
    AnalysisSettings::setKifuAnalysisEndPly(ui->spinBoxEndPly->value());
    DialogUtils::saveDialogSize(this, AnalysisSettings::setKifuAnalysisDialogSize);
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
    QSettings settings(SettingsCommon::settingsFilePath(), QSettings::IniFormat);

    // エンジンの数を取得する。
    int size = settings.beginReadArray("Engines");

    // エンジンの数だけループする。
    for (int i = 0; i < size; i++) {
        // 現在のインデックスで配列の要素を設定する。
        settings.setArrayIndex(i);

        // エンジン名とディレクトリを取得する。
        Engine engine;
        engine.name = settings.value("name").toString();
        engine.path = settings.value("path").toString();

        // 棋譜解析ダイアログのエンジン選択リストにエンジン名を追加する。
        ui->comboBoxEngine1->addItem(engine.name);

        // エンジンリストにエンジンを追加する。
        m_engineList.append(engine);
    }

    // エンジン名のグループの配列の読み込みを終了する。
    settings.endArray();
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

// フォントサイズを適用
void KifuAnalysisDialog::applyFontSize()
{
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

    // コンボボックスのポップアップリストにも反映する
    const QList<QComboBox*> comboBoxes = findChildren<QComboBox*>();
    for (QComboBox* comboBox : std::as_const(comboBoxes)) {
        if (comboBox && comboBox->view()) {
            comboBox->view()->setFont(f);
        }
    }
}
