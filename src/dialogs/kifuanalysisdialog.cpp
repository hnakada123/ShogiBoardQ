#include "kifuanalysisdialog.h"
#include "changeenginesettingsdialog.h"
#include "ui_kifuanalysisdialog.h"
#include "enginesettingsconstants.h"
#include "shogiutils.h"

#include <QFile>
#include <QTextStream>
#include <qmessagebox.h>

using namespace EngineSettingsConstants;

// 棋譜解析ダイアログのUIを設定する。
// コンストラクタ
KifuAnalysisDialog::KifuAnalysisDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::KifuAnalysisDialog), m_engineNumber(0), m_initPosition(true), m_byoyomiSec(0)
{
    // UIをセットアップする。
    ui->setupUi(this);

    // "開始局面から"にチェックを入れる。
    ui->radioButtonInitPosition->setChecked(true);

    // 設定ファイルからエンジンの名前とディレクトリを読み込む。
    readEngineNameAndDir();

    // エンジン設定ボタンが押されたときの処理
    connect(ui->engineSetting, &QPushButton::clicked, this, &KifuAnalysisDialog::showEngineSettingsDialog);

    // OKボタンが押された場合、ダイアログを受け入れたとして閉じる動作を行う。
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &KifuAnalysisDialog::accept);

    // OKボタンが押された場合、エンジン名、エンジン番号、解析局面フラグ、思考時間を取得する。
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &KifuAnalysisDialog::processEngineSettings);

    // キャンセルボタンが押された場合、ダイアログを拒否する動作を行う。
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &KifuAnalysisDialog::reject);
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

    // "開始局面から"にチェックが入っている場合
    if (ui->radioButtonInitPosition->isChecked()) {
        m_initPosition = true;
    }
    // "現在局面から"にチェックが入っている場合
    else if (ui->radioButtonCurrentPosition->isChecked()) {
        m_initPosition = false;
    }

    // 1手あたりの思考時間（秒数）を取得する。
    m_byoyomiSec = ui->byoyomiSec->text().toInt();
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

// "開始局面から"を選択したかどうかのフラグを取得する。
bool KifuAnalysisDialog::initPosition() const
{
    return m_initPosition;
}

// 設定ファイルからエンジンの名前とディレクトリを読み込む。
void KifuAnalysisDialog::readEngineNameAndDir()
{
    // 現在のディレクトリをアプリケーションのディレクトリに設定する。
    QDir::setCurrent(QApplication::applicationDirPath());

    // 設定ファイルを指定する。
    QSettings settings(SettingsFileName, QSettings::IniFormat);

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
