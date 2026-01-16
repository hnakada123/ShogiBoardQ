#ifndef STARTGAMEDIALOG_H
#define STARTGAMEDIALOG_H

#include <QDialog>
#include <QComboBox>
#include <QRadioButton>

namespace Ui {
class StartGameDialog;
}

class StartGameDialog : public QDialog
{
    Q_OBJECT

public:
    // コンストラクタ
    explicit StartGameDialog(QWidget *parent = nullptr);

    // デストラクタ
    ~StartGameDialog() override;

    // 先手／下手が人間であるかを示すフラグを取得する。
    bool isHuman1() const;

    // 後手／上手が人間であるかを示すフラグを取得する。
    bool isHuman2() const;

    // 先手／下手がエンジンであるかを示すフラグを取得する。
    bool isEngine1() const;

    // 後手／上手がエンジンであるかを示すフラグを取得する。
    bool isEngine2() const;

    // 先手／下手のエンジン名を取得する。
    const QString& engineName1() const;

    // 後手／上手のエンジン名を取得する。
    const QString& engineName2() const;

    // 先手／下手の人間名を取得する。
    const QString& humanName1() const;

    // 後手／上手の人間名を取得する。
    const QString& humanName2() const;

    // 先手／下手のエンジン番号を取得する。
    int engineNumber1() const;

    // 後手／上手のエンジン番号を取得する。
    int engineNumber2() const;

    // 対局者1の持ち時間の時間を取得する。
    int basicTimeHour1() const;

    // 対局者1の持ち時間の分を取得する。
    int basicTimeMinutes1() const;

    // 対局者1の秒読みの時間（秒）を取得する。
    int byoyomiSec1() const;

    // 対局者1の1手ごとの加算（秒）を取得する。
    int addEachMoveSec1() const;

    // 対局者2の持ち時間の時間を取得する。
    int basicTimeHour2() const;

    // 対局者2の持ち時間の分を取得する。
    int basicTimeMinutes2() const;

    // 対局者2の秒読みの時間（秒）を取得する。
    int byoyomiSec2() const;

    // 対局者2の1手ごとの加算（秒）を取得する。
    int addEachMoveSec2() const;

    // 対局の初期局面名を取得する。
    const QString& startingPositionName() const;

    // 対局の初期局面番号を取得する。
    int startingPositionNumber() const;

    // エンジンの名前とディレクトリを格納する構造体
    struct Engine
    {
        QString name;
        QString path;
    };

    // エンジンの名前とディレクトリを格納するリストを取得する。
    const QList<StartGameDialog::Engine>& getEngineList() const;

    // 最大手数を取得する。
    int maxMoves() const;

    // 連続対局数を取得する。
    int consecutiveGames() const;

    // 人を手前に表示するかどうかのフラグを取得する。
    bool isShowHumanInFront() const;

    // 棋譜の自動保存フラグを取得する。
    bool isAutoSaveKifu() const;

    // 時間切れを負けにするかどうかのフラグを取得する。
    bool isLoseOnTimeout() const;

    // 1局ごとに手番を入れ替えるかどうかのフラグを取得する。
    bool isSwitchTurnEachGame() const;

private:
    // UI
    Ui::StartGameDialog* ui;

    // 先手／下手が人間であるかを示すフラグ
    bool m_isHuman1;

    // 後手／上手が人間であるかを示すフラグ
    bool m_isHuman2;

    // 先手／下手がエンジンであるかを示すフラグ
    bool m_isEngine1;

    // 後手／上手がエンジンであるかを示すフラグ
    bool m_isEngine2;

    // 先手／下手のエンジン名
    QString m_engineName1;

    // 後手／上手のエンジン名
    QString m_engineName2;

    // 先手／下手の人間名
    QString m_humanName1;

    // 後手／上手の人間名
    QString m_humanName2;

    // 先手／下手のエンジン番号
    int m_engineNumber1;

    // 後手／上手のエンジン番号
    int m_engineNumber2;

    // 対局者1の持ち時間の時間
    int m_basicTimeHour1;

    // 対局者1の持ち時間の分
    int m_basicTimeMinutes1;

    // 対局者1の1手ごとの加算（秒）
    int m_addEachMoveSec1;

    // 対局者1の秒読みの時間（秒）
    int m_byoyomiSec1;

    // 対局者2の持ち時間の時間
    int m_basicTimeHour2;

    // 対局者2の持ち時間の分
    int m_basicTimeMinutes2;

    // 対局者2の1手ごとの加算（秒）
    int m_addEachMoveSec2;

    // 対局者2の秒読みの時間（秒）
    int m_byoyomiSec2;

    // 対局の初期局面
    QString m_startingPositionName;

    // 対局の初期局面番号
    int m_startingPositionNumber;

    // エンジン名とディレクトリを格納するリスト
    QList<Engine> engineList;

    // 最大手数
    int m_maxMoves;

    // 連続対局数
    int m_consecutiveGames;

    // 人を手前に表示するかどうかのフラグ
    bool m_isShowHumanInFront;

    // 棋譜の自動保存フラグ
    bool m_isAutoSaveKifu;

    // 時間切れを負けにするかどうかのフラグ
    bool m_isLoseOnTimeout;

    // 1局ごとに手番を入れ替えるかどうかのフラグ
    bool m_isSwitchTurnEachGame;

    // 設定ファイルからエンジンの名前とディレクトリを読み込む。
    void loadEngineConfigurations();

    // UIにエンジン設定を反映する。
    void populateUIWithEngines() const;

    // エンジン設定ダイアログの共通処理を行う。
    void showEngineSettingsDialog(QComboBox* comboBox);

    // シグナルとスロットを接続する。
    void connectSignalsAndSlots() const;

    // 設定ファイルから対局設定を読み込みGUIに反映する。
    void loadGameSettings();

private slots:
    // OKボタンが押された場合、対局ダイアログ内の各パラメータを取得する。
    void updateGameSettingsFromDialog();

    // 先手／下手のエンジン設定ボタンが押された場合、エンジン設定ダイアログを表示する。
    void onFirstPlayerSettingsClicked();

    // 後手／上手のエンジン設定ボタンが押された場合、エンジン設定ダイアログを表示する。
    void onSecondPlayerSettingsClicked();

    // 先後入れ替えボタンが押された場合、先後を入れ替える。
    void swapSides();

    // 設定ファイルに対局設定を保存する。
    void saveGameSettings();

    // 設定を初期値にリセットする。
    void resetSettingsToDefault();

    // 秒読みの値が変更された場合、1手ごとの加算時間を0に設定する。
    void handleByoyomiSecChanged(int value);

    // 1手ごとの加算時間の値が変更された場合、秒読みを0に設定する。
    void handleAddEachMoveSecChanged(int value);

    // 文字サイズを大きくする。
    void increaseFontSize();

    // 文字サイズを小さくする。
    void decreaseFontSize();

private:
    // 現在の文字サイズ
    int m_fontSize;

    // デフォルトの文字サイズ
    static constexpr int DefaultFontSize = 9;

    // 最小文字サイズ
    static constexpr int MinFontSize = 7;

    // 最大文字サイズ
    static constexpr int MaxFontSize = 16;

    // 文字サイズを適用する。
    void applyFontSize(int size);

    // 文字サイズ設定を読み込む。
    void loadFontSizeSettings();

    // 文字サイズ設定を保存する。
    void saveFontSizeSettings();
};

#endif // STARTGAMEDIALOG_H
