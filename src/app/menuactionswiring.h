#ifndef MENUACTIONSWIRING_H
#define MENUACTIONSWIRING_H

/// @file menuactionswiring.h
/// @brief メニュー・ツールバーの項目を、処理する部品へつなぐ

namespace Ui { class MainWindow; }
class AnalysisSession;
class BoardInteractionController;
class DialogLauncher;
class GameSession;
class ImageExportController;
class KifuAnalysisController;
class KifuFileController;
class MainWindowActions;
class MainWindowAppearanceController;
class PieceSoundPlayer;
class PositionEditCoordinator;
class SettingsResetController;
class TsumeSearchController;

/**
 * @brief メニュー・ツールバーの QAction を処理する部品のスロットへつなぐ
 *
 * 接続だけを行う。言語・表記のメニューは LanguageController、局面編集中の盤のメニューは
 * PositionEditCoordinator、ドックの配置のメニューは DockUiCoordinator がつなぐ。
 */
namespace MenuActionsWiring {

/// つなぐ先（すべて必須、非所有）
struct Deps {
    Ui::MainWindow* ui = nullptr;
    MainWindowActions* actions = nullptr;               ///< 新規・終了・局面編集の開始
    DialogLauncher* dialogs = nullptr;
    KifuFileController* kifuFiles = nullptr;           ///< 開く・保存・貼り付け・棋譜と局面のコピー
    GameSession* session = nullptr;                    ///< 対局の開始・投了・中断・待った・即指し
    AnalysisSession* analysis = nullptr;               ///< 詰み探索の中止
    PositionEditCoordinator* positionEdit = nullptr;   ///< 局面編集の終了
    KifuAnalysisController* kifuAnalysis = nullptr;    ///< 棋譜解析
    TsumeSearchController* tsumeSearch = nullptr;      ///< 詰み探索
    MainWindowAppearanceController* appearance = nullptr; ///< 盤の反転・拡大・縮小、ツールバー
    ImageExportController* images = nullptr;           ///< 盤・評価値グラフの画像
    BoardInteractionController* boardController = nullptr; ///< 合法手の表示
    PieceSoundPlayer* pieceSound = nullptr;            ///< 駒音
    SettingsResetController* settingsReset = nullptr;  ///< 設定の初期化
};

/// 起動時に1回だけ呼ぶ
void wire(const Deps& deps);

} // namespace MenuActionsWiring

#endif // MENUACTIONSWIRING_H
