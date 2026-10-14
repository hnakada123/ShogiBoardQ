#ifndef MAINWINDOW_H
#define MAINWINDOW_H

/// @file mainwindow.h
/// @brief MainWindow の宣言

#include <QMainWindow>
#include <memory>

class MainWindowComposer;

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

/**
 * @brief アプリのメインウィンドウ
 *
 * 部品の生成・配線・状態は MainWindowComposer が持つ。MainWindow はウィンドウ固有の
 * 責務（.ui の保持、閉じるときの確認と終了処理）だけを受け持つ。
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    /// 自動化 API（--automation）の待ち受けを開始する
    void startAutomationServer(const QString& socketPath);

    /// 未保存の確認をせずに閉じてアプリを終了する（自動化 API の終了要求用）
    void quitWithoutConfirmation();

    /// 部品と状態（テストと自動化 API から参照する）
    MainWindowComposer& composer() { return *m_composer; }

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    std::unique_ptr<Ui::MainWindow> ui;
    std::unique_ptr<MainWindowComposer> m_composer;
    bool m_skipCloseConfirmation = false;
};

#endif // MAINWINDOW_H
