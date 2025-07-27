#ifndef CONSIDARATIONDIALOG_H
#define CONSIDARATIONDIALOG_H

#include <QDialog>

namespace Ui {
class ConsidarationDialog;
}

// 検討ダイアログを表示する。
class ConsidarationDialog : public QDialog
{
    Q_OBJECT

public:
    // コンストラクタ
    explicit ConsidarationDialog(QWidget *parent = nullptr);

    // デストラクタ
    ~ConsidarationDialog();

    // 1手あたりの思考時間（秒数）を取得する。
     int getByoyomiSec() const;

    // エンジン番号を取得する。
    int getEngineNumber() const;

    // エンジン名を取得する。
    const QString& getEngineName() const;

    // エンジンの名前とディレクトリを格納する構造体
    struct Engine
    {
        QString name;
        QString path;
    };

    // エンジンの名前とディレクトリを格納するリストを取得する。
    const QList<ConsidarationDialog::Engine>& getEngineList() const;

    bool unlimitedTimeFlag() const;

private slots:
    // エンジン設定ボタンが押された場合、エンジン設定ダイアログを表示する。
    void showEngineSettingsDialog();

    // エンジン名、エンジン番号、時間無制限フラグ、検討時間フラグ、検討時間を取得する。
    void processEngineSettings();

private:
    // UI
    Ui::ConsidarationDialog* ui;

    // 選択したエンジン名
    QString m_engineName;

    // 選択したエンジン番号
    int m_engineNumber;

    // "時間無制限"を選択した場合、true
    bool m_unlimitedTimeFlag;

    // 1手あたりの思考時間（秒数）
    int m_byoyomiSec;

    // エンジンの名前とディレクトリを格納するリスト
    QList<Engine> engineList;

    // 設定ファイルからエンジンの名前とディレクトリを読み込む。
    void readEngineNameAndDir();
};

inline bool ConsidarationDialog::unlimitedTimeFlag() const
{
    return m_unlimitedTimeFlag;
}

#endif // CONSIDARATIONDIALOG_H
