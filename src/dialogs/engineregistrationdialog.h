#ifndef ENGINEREGISTRATIONDIALOG_H
#define ENGINEREGISTRATIONDIALOG_H

/// @file engineregistrationdialog.h
/// @brief エンジン登録ダイアログクラスの定義

#include <QDialog>
#include <memory>

namespace Ui {
class EngineRegistrationDialog;
}

class EngineRegistrationHandler;

// エンジン登録ダイアログ
class EngineRegistrationDialog : public QDialog
{
    Q_OBJECT

public:
    // コンストラクタ
    explicit EngineRegistrationDialog(QWidget *parent = nullptr);

    // デストラクタ
    ~EngineRegistrationDialog() override;

private slots:
    // 追加ボタンが押されたときに呼び出されるスロット
    void addEngineFromFileSelection();

    // 削除ボタンが押されたときに呼び出されるスロット
    void removeEngine();

    // 設定ボタンが押されたときに呼び出されるスロット
    void configureEngine();

    /// 画面の一覧をハンドラの一覧（正本）から作り直す（選んでいたエンジンは名前で選び直す）
    void rebuildEngineList();

    // エンジン登録完了時のスロット
    void onEngineRegistered(const QString& engineName);

    // ハンドラエラー時のスロット
    void onHandlerError(const QString& errorMessage);

    // 登録処理の進行状態変化時のスロット
    void onRegistrationInProgressChanged(bool inProgress);
    // 選んだエンジンに合わせて削除・設定ボタンを有効にする
    void updateSelectionState();

private:
    std::unique_ptr<Ui::EngineRegistrationDialog> ui;
    EngineRegistrationHandler* m_handler = nullptr;   ///< 登録の処理（親はこのダイアログ）

    // シグナル・スロットの接続を行う。
    void initializeSignals() const;
};

#endif // ENGINEREGISTRATIONDIALOG_H
