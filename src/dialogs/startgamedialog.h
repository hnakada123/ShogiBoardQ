#ifndef STARTGAMEDIALOG_H
#define STARTGAMEDIALOG_H

/// @file startgamedialog.h
/// @brief 対局開始ダイアログクラスの定義

#include <QDialog>

#include <array>
#include <memory>

#include "settingsformbinding.h"
#include "startgamedatabridge.h"

namespace Ui {
class StartGameDialog;
}

class PlayerSetupWidget;
class TimeControlEditor;

/**
 * @brief 対局開始時の条件を選ぶダイアログ
 *
 * 先手・後手それぞれの対局者（PlayerSetupWidget）と時間（TimeControlEditor）、開始局面・最大手数・
 * 連続対局・棋譜の自動保存を選ぶ。選んだ条件は data() で値として渡す。
 */
class StartGameDialog : public QDialog
{
    Q_OBJECT

public:
    explicit StartGameDialog(QWidget *parent = nullptr);
    ~StartGameDialog() override;

    /// 画面で選んだ対局の条件（「後手に異なる時間を設定」がなければ後手の時間は先手と同じ）
    StartGameDialogData data() const;

    /// 開始局面を「現在の局面」にする
    void forceCurrentPositionSelection();

private:
    std::unique_ptr<Ui::StartGameDialog> ui; ///< UIオブジェクト

    std::array<PlayerSetupWidget*, 2> m_players{};  ///< 先手／下手・後手／上手の対局者（親が所有）
    std::array<TimeControlEditor*, 2> m_times{};    ///< 先手／下手・後手／上手の時間（親が所有）
    SettingsFormBinding m_form;                     ///< 対局者・時間・開始局面以外の欄と設定の対応

    /// 対局者・時間の欄を作り、グループに入れる
    void buildPlayerSections();
    /// 開始局面の一覧（「現在の局面」と手合割の表）
    void populateStartingPositions();
    /// 入力欄と設定の対応を作る
    void bindSettings();

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

    /// 選んだエンジンの設定ダイアログを表示する
    void showEngineSettingsDialog(const EngineListSettings::EngineEntry& engine);

    /// 先後の対局者設定を入れ替える
    void swapSides();

    /// 現在の対局設定を設定ファイルに保存する
    void saveGameSettings();

    /// 設定だけを保存し、画面内に完了を表示する
    void saveSettingsOnly();

    /// 設定を初期値にリセットする
    void resetSettingsToDefault();

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
};

#endif // STARTGAMEDIALOG_H
