#ifndef MENUACTIONSWIRING_H
#define MENUACTIONSWIRING_H

/// @file menuactionswiring.h
/// @brief メニュー・ツールバーの項目を、処理する部品へつなぐ

namespace Ui { class MainWindow; }
class AnalysisSession;
class BoardInteractionController;
class CsaGameSession;
class DialogLauncher;
class DockLayoutManager;
class GameSession;
class ImageExportController;
class KifuAnalysisController;
class KifuFileController;
class LanguageController;
class MainWindowActions;
class MainWindowAppearanceController;
class MenuWindowController;
class PieceSoundPlayer;
class PositionEditSession;
class SettingsResetController;
class TsumeSearchController;

/**
 * @brief メニュー・ツールバーの QAction を処理する部品のスロットへつなぐ
 *
 * メニュー・ツールバーの項目の triggered・toggled は、すべてここでつなぐ。
 * チェックできる項目（合法手の表示・ツールバー・駒音・ドックの固定・移動元）は、設定を持つ部品の今の値から
 * 始め、変えたらその部品に渡す（保存は部品が行う）。ここでつながないのは次の2つだけ。
 * - 排他の組（言語・指し手の表記）: 組と選択の印を持つ LanguageController
 * - 項目を作る部品のメニュー（保存したレイアウトの一覧）: DockLayoutManager
 * 「盤に定跡手を表示」は矢印の切り替え（BoardArrowActions）が正本で、保存もそこが行う。
 */
namespace MenuActionsWiring {

/// つなぐ先（すべて必須、非所有）
struct Deps {
    Ui::MainWindow* ui = nullptr;
    MainWindowActions* actions = nullptr;               ///< 新規・終了・局面編集の開始
    DialogLauncher* dialogs = nullptr;
    KifuFileController* kifuFiles = nullptr;           ///< 開く・保存・貼り付け・棋譜と局面のコピー
    GameSession* session = nullptr;                    ///< 対局の開始・投了・中断・待った・即指し
    CsaGameSession* csaSession = nullptr;              ///< 通信対局の接続ダイアログ
    MenuWindowController* menuWindow = nullptr;        ///< メニューウィンドウの表示の切り替え
    AnalysisSession* analysis = nullptr;               ///< 詰み探索の中止
    PositionEditSession* positionEdit = nullptr;   ///< 局面編集の終了
    KifuAnalysisController* kifuAnalysis = nullptr;    ///< 棋譜解析
    TsumeSearchController* tsumeSearch = nullptr;      ///< 詰み探索
    MainWindowAppearanceController* appearance = nullptr; ///< 盤の反転・拡大・縮小、ツールバー
    ImageExportController* images = nullptr;           ///< 盤・評価値グラフの画像
    BoardInteractionController* boardController = nullptr; ///< 合法手の表示
    PieceSoundPlayer* pieceSound = nullptr;            ///< 駒音
    SettingsResetController* settingsReset = nullptr;  ///< 設定の初期化
    DockLayoutManager* dockLayout = nullptr;           ///< ドックの配置の初期化・保存・固定
    LanguageController* language = nullptr;            ///< 指し手の表記の移動元・読み方の説明
};

/// 起動時に1回だけ呼ぶ
void wire(const Deps& deps);

} // namespace MenuActionsWiring

#endif // MENUACTIONSWIRING_H
