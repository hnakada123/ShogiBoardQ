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

// エンジンの構造体
struct Engine
{
    // エンジン名
    QString name;

    // エンジンの実行ファイル名
    QString path;

    // エンジンの作者名
    QString author;
};

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

    // エンジン登録完了時のスロット
    void onEngineRegistered(const QString& engineName);

    // ハンドラエラー時のスロット
    void onHandlerError(const QString& errorMessage);

    // 登録処理の進行状態変化時のスロット
    void onRegistrationInProgressChanged(bool inProgress);

    // フォントサイズを増加する
    void increaseFontSize();

    // フォントサイズを減少する
    void decreaseFontSize();

private:
    std::unique_ptr<Ui::EngineRegistrationDialog> ui;
    std::unique_ptr<EngineRegistrationHandler> m_handler;

    // 現在のフォントサイズ
    int m_fontSize = 0;

    // シグナル・スロットの接続を行う。
    void initializeSignals() const;

    // すべてのウィジェットにフォントサイズを適用する
    void applyFontSize();
};

#endif // ENGINEREGISTRATIONDIALOG_H
