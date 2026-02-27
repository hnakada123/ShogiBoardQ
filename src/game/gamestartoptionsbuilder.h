#ifndef GAMESTARTOPTIONSBUILDER_H
#define GAMESTARTOPTIONSBUILDER_H

/// @file gamestartoptionsbuilder.h
/// @brief ダイアログ値解釈・PlayMode判定・開始局面プリセットを担うユーティリティ

#include <QString>

#include "gamestartcoordinator.h"
#include "playmode.h"

class QObject;
class QWidget;
class QDialog;
class ShogiView;
class ShogiGameController;

/**
 * @brief 対局開始パラメータの構築を担う static ユーティリティクラス
 *
 * GameStartCoordinator から以下の責務を分離:
 *  - ダイアログの値解釈（TimeControl / PlayMode の抽出）
 *  - PlayMode 判定ロジック
 *  - 開始局面プリセットテーブル
 *  - 対局者名・盤面適用のユーティリティ
 */
class GameStartOptionsBuilder
{
public:
    using TimeControl = GameStartCoordinator::TimeControl;

    // --- TimeControl 構築 ---

    /// QWidget の子オブジェクトの property から TimeControl を組み立てる
    static TimeControl extractTimeControl(const QWidget* dlg);

    /// StartGameDialog (QDialog*) から TimeControl を組み立てる
    static TimeControl buildTimeControl(QDialog* startDlg);

    // --- PlayMode 判定 ---

    /// 平手/駒落ち × 人/エンジン から PlayMode を決定する
    static PlayMode determinePlayMode(int initPositionNumber,
                                      bool isPlayer1Human,
                                      bool isPlayer2Human);

    /// ダイアログの状態から PlayMode を決定する
    static PlayMode determinePlayModeFromDialog(QDialog* startDlg);

    /// SFENの手番とダイアログ設定を整合させてPlayModeを決定する
    static PlayMode determinePlayModeAlignedWithTurn(
        int initPositionNumber,
        bool isPlayer1Human,
        bool isPlayer2Human,
        const QString& startSfen);

    // --- 開始局面プリセット ---

    /// 手合割インデックス（1=平手, 2=香落ち, …, 14=十枚落ち）から純SFENを返す
    static const QString& startingPositionSfen(int index);

    /// プリセット数
    static constexpr int kStartingPositionCount = 14;

    // --- SFEN 正規化 ---

    /// "startpos" や空文字列を平手SFENに正規化する
    static QString canonicalizeSfen(const QString& sfen);

    // --- ユーティリティ ---

    /// PlayModeに応じて盤面ビューに対局者名を設定する
    static void applyPlayersNamesForMode(ShogiView* view,
                                         PlayMode mode,
                                         const QString& human1,
                                         const QString& human2,
                                         const QString& engine1,
                                         const QString& engine2);

    /// 再開SFENが指定されていれば盤へ適用し、即時描画まで行う
    static void applyResumePositionIfAny(ShogiGameController* gc,
                                         ShogiView* view,
                                         const QString& resumeSfen);

private:
    static int  readIntProperty(const QObject* root, const char* objectName,
                                const char* prop = "value", int def = 0);
    static bool readBoolProperty(const QObject* root, const char* objectName,
                                 const char* prop = "checked", bool def = false);
    static QChar turnFromSfen(const QString& sfen);
};

#endif // GAMESTARTOPTIONSBUILDER_H
