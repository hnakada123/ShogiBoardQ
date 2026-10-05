/// @file settingsresetcontroller.cpp
/// @brief 全設定の初期化と終了確認の実装

#include "settingsresetcontroller.h"
#include "dialogutils.h"
#include "settingscommon.h"

#include <QCoreApplication>
#include <QMessageBox>
#include <QWidget>

SettingsResetController::SettingsResetController(QWidget* parentWindow)
    : QObject(parentWindow)
    , m_parentWindow(parentWindow)
{
}

void SettingsResetController::confirmAndQuit()
{
    const bool confirmed = DialogUtils::confirmAction(
        m_parentWindow, tr("設定の初期化"),
        tr("エンジン登録、表示、対局など、すべての設定を初期値に戻して終了します。\n"
           "この操作は取り消せません。次回起動時は初期設定が使用されます。\n\n"
           "設定を初期値に戻して終了しますか？"),
        tr("初期値に戻して終了"), QMessageBox::Warning);
    if (!confirmed || !m_parentWindow) return;

    // 棋譜・定跡の未保存確認を含む通常の終了処理を通す。
    // 終了がキャンセルされた場合は設定を初期化しない。
    if (m_parentWindow->close()) {
        QCoreApplication::exit(ResetSettingsExitCode);
    }
}

int SettingsResetController::finalizeExit(int exitCode)
{
    if (exitCode != ResetSettingsExitCode) return exitCode;

    // ウィンドウや子ウィジェットが終了時に保存する値も、ここでまとめて消去する。
    if (SettingsCommon::resetAllSettings()) return 0;

    QMessageBox::critical(nullptr, tr("設定の初期化に失敗"),
        tr("設定ファイルを初期化できませんでした。\n"
           "保存先の書き込み権限や空き容量を確認してください。\n\n%1")
            .arg(SettingsCommon::settingsFilePath()));
    return 1;
}
