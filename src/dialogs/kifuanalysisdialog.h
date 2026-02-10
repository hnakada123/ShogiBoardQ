#ifndef KIFUANALYSISDIALOG_H
#define KIFUANALYSISDIALOG_H

/// @file kifuanalysisdialog.h
/// @brief 棋譜解析ダイアログクラスの定義


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

    // "開始局面から最終手まで"を選択したかどうかのフラグを取得する。
    bool initPosition() const;
    
    // 範囲指定の開始手数を取得する（0から）
    int startPly() const;
    
    // 範囲指定の終了手数を取得する
    int endPly() const;
    
    // 最大手数を設定する（ダイアログ表示前に呼び出す）
    void setMaxPly(int maxPly);

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
    
    // 範囲指定ラジオボタンが選択された場合
    void onRangeRadioToggled(bool checked);
    
    // 開始手数が変更された場合
    void onStartPlyChanged(int value);
    
    // フォントサイズ拡大
    void onFontIncrease();
    
    // フォントサイズ縮小
    void onFontDecrease();

private:
    // UI
    Ui::KifuAnalysisDialog* ui;

    // 選択したエンジン名
    QString m_engineName;

    // 選択したエンジン番号
    int m_engineNumber = 0;

    // "開始局面から最終手まで"を選択した場合、true
    bool m_initPosition = true;

    // 範囲指定の開始手数
    int m_startPly = 0;

    // 範囲指定の終了手数
    int m_endPly = 0;

    // 最大手数
    int m_maxPly = 0;

    // 設定から読み込んだ開始・終了手数（setMaxPly時に適用）
    int m_savedStartPly = 0;
    int m_savedEndPly = 0;

    // 1手あたりの思考時間（秒数）
    int m_byoyomiSec = 0;

    // 現在のフォントサイズ（ポイント）
    int m_fontSize = 0;

    // エンジンの名前とディレクトリを格納するリスト
    QList<Engine> m_engineList;

    // 設定ファイルからエンジンの名前とディレクトリを読み込む。
    void readEngineNameAndDir();
    
    // フォントサイズを適用
    void applyFontSize();
};

#endif // KIFUANALYSISDIALOG_H
