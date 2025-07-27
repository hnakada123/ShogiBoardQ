#ifndef KIFUANALYSISDIALOG_H
#define KIFUANALYSISDIALOG_H

#include <QDialog>

namespace Ui {
class KifuAnalysisDialog;
}

// 棋譜解析ダイアログのUIを設定する。
class KifuAnalysisDialog : public QDialog
{
    Q_OBJECT

public:
    // コンストラクタ
    explicit KifuAnalysisDialog(QWidget *parent = nullptr);

    // エンジンの名前とディレクトリを格納する構造体
    struct Engine
    {
        QString name;
        QString path;
    };

    // エンジンの名前とディレクトリを格納するリストを取得する。
    QList<Engine> engineList() const;

    // "開始局面から"を選択したかどうかのフラグを取得する。
    bool initPosition() const;

    // 1手あたりの思考時間（秒数）を取得する。
    int byoyomiSec() const;

     // エンジン番号を取得する。
    int engineNumber() const;

    // エンジン名を取得する。
    QString engineName() const;

private slots:
    // エンジン設定ボタンが押された場合、エンジン設定ダイアログを表示する。
    void showEngineSettingsDialog();

    // OKボタンが押された場合、エンジン名、エンジン番号、解析局面フラグ、思考時間を取得する。
    void processEngineSettings();

private:
    // UI
    Ui::KifuAnalysisDialog* ui;

    // 選択したエンジン名
    QString m_engineName;

    // 選択したエンジン番号
    int m_engineNumber;

    // "開始局面から"を選択した場合、true
    bool m_initPosition;

    // 1手あたりの思考時間（秒数）
    int m_byoyomiSec;

    // エンジンの名前とディレクトリを格納するリスト
    QList<Engine> m_engineList;

    // 設定ファイルからエンジンの名前とディレクトリを読み込む。
    void readEngineNameAndDir();
};

#endif // KIFUANALYSISDIALOG_H
