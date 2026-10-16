#ifndef STARTGAMEDIALOG_H
#define STARTGAMEDIALOG_H

/// @file startgamedialog.h
/// @brief 対局開始ダイアログクラスの定義

#include "enginelistsettings.h"
#include <QDialog>
#include <QComboBox>

#include <memory>

#include "fontsizehelper.h"
#include "startgamedatabridge.h"

namespace Ui {
class StartGameDialog;
}

/**
 * @brief 対局開始時のパラメータを収集するダイアログ
 *
 * 対局者（人間/エンジン）の選択、持ち時間設定、開始局面選択、
 * 連続対局設定などをユーザーから取得し、StartGameDialogのプロパティとして保持する。
 *
 */
class StartGameDialog : public QDialog
{
    Q_OBJECT

public:
    explicit StartGameDialog(QWidget *parent = nullptr);
    ~StartGameDialog() override;

    /// 画面で選んだ対局の条件（「後手に異なる時間を設定」がなければ後手の時間は先手と同じ）
    StartGameDialogData data() const;

    /// 開始局面コンボボックスを「現在の局面」(index 0) に強制設定する
    void forceCurrentPositionSelection();

    // --- エンジン情報 ---

    /// エンジンの名前とディレクトリを格納する構造体
    /// 登録済みエンジン（名前・実行ファイル・作者）
    using Engine = EngineListSettings::EngineEntry;

    const QList<StartGameDialog::Engine>& engineList() const;

private:
    std::unique_ptr<Ui::StartGameDialog> ui; ///< UIオブジェクト

    QList<Engine> m_engineList;              ///< 登録エンジンのリスト

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
    void connectSignalsAndSlots();

    /// 設定ファイルから対局設定を読み込みGUIに反映する
    void loadGameSettings();

    /// 取得済みの時間設定で対局を開始できるか確認し、できなければ理由を表示する
    /// @return 開始できるなら true
    bool confirmTimeSettings();

private slots:
    /// 対局開始ボタン押下時に、時間設定を確認してから設定を保存しダイアログを閉じる
    void onStartRequested();

    /// 先手側のエンジン設定ダイアログを表示する
    void onFirstPlayerSettingsClicked();

    /// 後手側のエンジン設定ダイアログを表示する
    void onSecondPlayerSettingsClicked();

    /// 先後の対局者設定を入れ替える
    void swapSides();

    /// 現在の対局設定を設定ファイルに保存する
    void saveGameSettings();

    /// 設定だけを保存し、画面内に完了を表示する
    void saveSettingsOnly();

    /// 設定を初期値にリセットする
    void resetSettingsToDefault();

    /// 秒読み値変更時に加算時間を0にする（秒読みと加算は排他）
    void handleByoyomiSecChanged(int value);

    /// 加算時間値変更時に秒読みを0にする（秒読みと加算は排他）
    void handleAddEachMoveSecChanged(int value);

    /// 先手側の対局者選択変更時にUIを更新する
    void onPlayer1SelectionChanged(int index);

    /// 後手側の対局者選択変更時にUIを更新する
    void onPlayer2SelectionChanged(int index);

    /// 棋譜保存ディレクトリ選択ダイアログを表示する
    void onSelectKifuDirClicked();

    /// 連続対局スピンボックスの有効/無効を更新する（両方エンジン時のみ有効）
    void updateConsecutiveGamesEnabled();

    void updateKifuSaveEnabled();
    void updateTimeSettingsTitle();

private:
    // --- フォント設定 ---

    static constexpr int MinFontSize = 7;               ///< 最小フォントサイズ
    static constexpr int MaxFontSize = 16;              ///< 最大フォントサイズ
    FontSizeHelper m_fontHelper;                        ///< 文字サイズ（既定値は設定の宣言の表）

    void applyFontSize();
};

#endif // STARTGAMEDIALOG_H
