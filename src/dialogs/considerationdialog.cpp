#include "considerationdialog.h"
#include "changeenginesettingsdialog.h"
#include "ui_considerationdialog.h"
#include "enginesettingsconstants.h"
#include <QFile>
#include <QTextStream>
#include <QMessageBox>

using namespace EngineSettingsConstants;

// 検討ダイアログを表示する。
// コンストラクタ
ConsiderationDialog::ConsiderationDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::ConsiderationDialog)
{
    // UIをセットアップする。
    ui->setupUi(this);

    // "開始局面から"にチェックを入れる。
    ui->unlimitedTimeRadioButton->setChecked(true);

    // 設定ファイルからエンジンの名前とディレクトリを読み込む。
    readEngineNameAndDir();

    // エンジン設定ボタンが押されたときの処理
    connect(ui->engineSetting, &QPushButton::clicked, this, &ConsiderationDialog::showEngineSettingsDialog);

    // OKボタンが押された場合、ダイアログを受け入れたとして閉じる動作を行う。
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &ConsiderationDialog::accept);

    // OKボタンが押された場合、エンジン名、エンジン番号、解析局面フラグ、思考時間を取得する。
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &ConsiderationDialog::processEngineSettings);

    // キャンセルボタンが押された場合、ダイアログを拒否する動作を行う。
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &ConsiderationDialog::reject);
}

// デストラクタ
ConsiderationDialog::~ConsiderationDialog()
{
    // UIを削除する。
    delete ui;
}

// エンジン設定ボタンが押された場合、エンジン設定ダイアログを表示する。
void ConsiderationDialog::showEngineSettingsDialog()
{
    // 選択したエンジン番号を取得する。
    m_engineNumber = ui->comboBoxEngine1->currentIndex();

    // 選択したエンジン名を取得する。
    m_engineName = ui->comboBoxEngine1->currentText();

    // エンジン名が空の場合
    if (m_engineName.isEmpty()) {
        // エラーメッセージを表示する。
        QString errorText = "将棋エンジンが選択されていません。";
        QMessageBox::critical(this, "エラー", errorText);
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

// エンジン名を取得する。
const QString& ConsiderationDialog::getEngineName() const
{
    return m_engineName;
}

// エンジン番号を取得する。
int ConsiderationDialog::getEngineNumber() const
{
    return m_engineNumber;
}

// 1手あたりの思考時間（秒数）を取得する。
int ConsiderationDialog::getByoyomiSec() const
{
    return m_byoyomiSec;
}

// エンジン名、エンジン番号、時間無制限フラグ、検討時間フラグ、検討時間を取得する。
void ConsiderationDialog::processEngineSettings()
{
    // 選択したエンジン名を取得する。
    m_engineName = ui->comboBoxEngine1->currentText();

    // 選択したエンジン番号を取得する。
    m_engineNumber = ui->comboBoxEngine1->currentIndex();

    // "時間無制限"にチェックが入っている場合
    if (ui->unlimitedTimeRadioButton->isChecked()) {
        m_unlimitedTimeFlag = true;
    }
    // "検討時間"にチェックが入っている場合
    else if (ui->considerationTimeRadioButton->isChecked()) {
        m_unlimitedTimeFlag = false;
        m_byoyomiSec = ui->byoyomiSec->text().toInt();
    }
}

// 設定ファイルからエンジンの名前とディレクトリを読み込む。
void ConsiderationDialog::readEngineNameAndDir()
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
        engineList.append(engine);
    }

    // エンジン名のグループの配列の読み込みを終了する。
    settings.endArray();
}

// エンジンの名前とディレクトリを格納するリストを取得する。
const QList<ConsiderationDialog::Engine>& ConsiderationDialog::getEngineList() const
{
    return engineList;
}
