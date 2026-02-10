#ifndef STARTGAMEDIALOG_H
#define STARTGAMEDIALOG_H

/// @file startgamedialog.h
/// @brief 対局開始ダイアログクラスの定義

#include <QDialog>
#include <QComboBox>

namespace Ui {
class StartGameDialog;
}

/**
 * @brief 対局開始時のパラメータを収集するダイアログ
 *
 * 対局者（人間/エンジン）の選択、持ち時間設定、開始局面選択、
 * 連続対局設定などをユーザーから取得し、StartGameDialogのプロパティとして保持する。
 *
 * @todo remove コメントスタイルガイド適用済み
 */
class StartGameDialog : public QDialog
{
    Q_OBJECT

public:
    explicit StartGameDialog(QWidget *parent = nullptr);
    ~StartGameDialog() override;

    // --- 対局者情報 ---

    bool isHuman1() const;
    bool isHuman2() const;
    bool isEngine1() const;
    bool isEngine2() const;
    const QString& engineName1() const;
    const QString& engineName2() const;
    const QString& humanName1() const;
    const QString& humanName2() const;
    int engineNumber1() const;
    int engineNumber2() const;

    // --- 持ち時間設定 ---

    int basicTimeHour1() const;
    int basicTimeMinutes1() const;
    int byoyomiSec1() const;
    int addEachMoveSec1() const;
    int basicTimeHour2() const;
    int basicTimeMinutes2() const;
    int byoyomiSec2() const;
    int addEachMoveSec2() const;

    // --- 開始局面 ---

    const QString& startingPositionName() const;
    int startingPositionNumber() const;

    // --- エンジン情報 ---

    /// エンジンの名前とディレクトリを格納する構造体
    struct Engine
    {
        QString name;  ///< エンジン表示名
        QString path;  ///< エンジン実行ファイルのパス
    };

    const QList<StartGameDialog::Engine>& getEngineList() const;

    // --- 対局オプション ---

    int maxMoves() const;
    int consecutiveGames() const;
    bool isShowHumanInFront() const;
    bool isAutoSaveKifu() const;
    const QString& kifuSaveDir() const;
    bool isLoseOnTimeout() const;
    bool isSwitchTurnEachGame() const;

    /// 持将棋ルールを取得する
    /// @return 0: なし, 1: 24点法, 2: 27点法
    int jishogiRule() const;

private:
    Ui::StartGameDialog* ui;                ///< UIオブジェクト

    // --- 対局者フラグ ---

    bool m_isHuman1 = false;                 ///< 先手／下手が人間か
    bool m_isHuman2 = false;                 ///< 後手／上手が人間か
    bool m_isEngine1 = false;                ///< 先手／下手がエンジンか
    bool m_isEngine2 = false;                ///< 後手／上手がエンジンか

    // --- 対局者名 ---

    QString m_engineName1;                  ///< 先手／下手のエンジン名
    QString m_engineName2;                  ///< 後手／上手のエンジン名
    QString m_humanName1;                   ///< 先手／下手の人間名
    QString m_humanName2;                   ///< 後手／上手の人間名
    int m_engineNumber1 = 0;                 ///< 先手／下手のエンジン番号
    int m_engineNumber2 = 0;                 ///< 後手／上手のエンジン番号

    // --- 持ち時間 ---

    int m_basicTimeHour1 = 0;                ///< 対局者1の持ち時間（時間）
    int m_basicTimeMinutes1 = 0;             ///< 対局者1の持ち時間（分）
    int m_addEachMoveSec1 = 0;               ///< 対局者1のフィッシャー加算（秒）
    int m_byoyomiSec1 = 0;                   ///< 対局者1の秒読み（秒）
    int m_basicTimeHour2 = 0;                ///< 対局者2の持ち時間（時間）
    int m_basicTimeMinutes2 = 0;             ///< 対局者2の持ち時間（分）
    int m_addEachMoveSec2 = 0;               ///< 対局者2のフィッシャー加算（秒）
    int m_byoyomiSec2 = 0;                   ///< 対局者2の秒読み（秒）

    // --- 局面・対局オプション ---

    QString m_startingPositionName;         ///< 開始局面名（平手、二枚落ち等）
    int m_startingPositionNumber = 0;        ///< 開始局面番号
    QList<Engine> engineList;               ///< 登録エンジンのリスト
    int m_maxMoves = 0;                      ///< 最大手数（0=無制限）
    int m_consecutiveGames = 1;              ///< 連続対局数
    bool m_isShowHumanInFront = false;       ///< 人間を手前に表示するか
    bool m_isAutoSaveKifu = false;           ///< 棋譜の自動保存フラグ
    QString m_kifuSaveDir;                  ///< 棋譜の保存ディレクトリ
    bool m_isLoseOnTimeout = false;          ///< 時間切れ負けフラグ
    bool m_isSwitchTurnEachGame = false;     ///< 1局ごとに手番入替フラグ
    int m_jishogiRule = 0;                   ///< 持将棋ルール（0:なし, 1:24点法, 2:27点法）

    // --- 初期化・設定 ---

    /// 設定ファイルからエンジンの名前とディレクトリを読み込む
    void loadEngineConfigurations();

    /// 対局者コンボボックスに人間＋エンジンリストを反映する
    void populatePlayerComboBoxes();

    /// エンジン設定ダイアログを表示する共通処理
    /// @param playerNumber 対局者番号（1=先手, 2=後手）
    void showEngineSettingsDialog(int playerNumber);

    /// 対局者コンボボックスの選択に応じてUIを更新する
    /// @param playerNumber 対局者番号（1=先手, 2=後手）
    /// @param index コンボボックスのインデックス（0=人間, 1以上=エンジン）
    void updatePlayerUI(int playerNumber, int index);

    /// ダイアログ内のUIパーツとスロットを結線する（コンストラクタから呼ばれる）
    void connectSignalsAndSlots() const;

    /// 設定ファイルから対局設定を読み込みGUIに反映する
    void loadGameSettings();

private slots:
    /// OKボタン押下時にダイアログの各パラメータをメンバー変数に取得する
    void updateGameSettingsFromDialog();

    /// 先手側のエンジン設定ダイアログを表示する
    void onFirstPlayerSettingsClicked();

    /// 後手側のエンジン設定ダイアログを表示する
    void onSecondPlayerSettingsClicked();

    /// 先後の対局者設定を入れ替える
    void swapSides();

    /// 現在の対局設定を設定ファイルに保存する
    void saveGameSettings();

    /// 設定を初期値にリセットする
    void resetSettingsToDefault();

    /// 秒読み値変更時に加算時間を0にする（秒読みと加算は排他）
    void handleByoyomiSecChanged(int value);

    /// 加算時間値変更時に秒読みを0にする（秒読みと加算は排他）
    void handleAddEachMoveSecChanged(int value);

    void increaseFontSize();
    void decreaseFontSize();

    /// 先手側の対局者選択変更時にUIを更新する
    void onPlayer1SelectionChanged(int index);

    /// 後手側の対局者選択変更時にUIを更新する
    void onPlayer2SelectionChanged(int index);

    /// 棋譜保存ディレクトリ選択ダイアログを表示する
    void onSelectKifuDirClicked();

    /// 連続対局スピンボックスの有効/無効を更新する（両方エンジン時のみ有効）
    void updateConsecutiveGamesEnabled();

private:
    // --- フォント設定 ---

    int m_fontSize = DefaultFontSize;                    ///< 現在のフォントサイズ
    static constexpr int DefaultFontSize = 9;           ///< デフォルトのフォントサイズ
    static constexpr int MinFontSize = 7;               ///< 最小フォントサイズ
    static constexpr int MaxFontSize = 16;              ///< 最大フォントサイズ

    void applyFontSize(int size);
    void loadFontSizeSettings();
    void saveFontSizeSettings();
};

#endif // STARTGAMEDIALOG_H
