#ifndef SETTINGSRESETCONTROLLER_H
#define SETTINGSRESETCONTROLLER_H

/// @file settingsresetcontroller.h
/// @brief 全設定の初期化と、それに伴う終了確認を担当するコントローラ

#include <QObject>

class QWidget;

class SettingsResetController : public QObject
{
    Q_OBJECT

public:
    explicit SettingsResetController(QWidget* parentWindow);

    /// 全ウィンドウの破棄後に呼び、初期化要求があれば設定を消去する。
    /// 通常の終了コードはそのまま返し、初期化時は成功なら0、失敗なら1を返す。
    static int finalizeExit(int exitCode);

public slots:
    void confirmAndQuit();

private:
    static constexpr int ResetSettingsExitCode = 1000;
    QWidget* m_parentWindow;
};

#endif // SETTINGSRESETCONTROLLER_H
