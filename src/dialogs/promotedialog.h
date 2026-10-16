#ifndef PROMOTEDIALOG_H
#define PROMOTEDIALOG_H

/// @file promotedialog.h
/// @brief 成り確認ダイアログクラスの定義


#include <QDialog>

#include <memory>

namespace Ui {
class PromoteDialog;
}

// 成る・不成の選択ダイアログを表示するクラス
class PromoteDialog : public QDialog
{
    Q_OBJECT

public:
    // コンストラクタ
    explicit PromoteDialog(QWidget* parent = nullptr);

    // デストラクタ
    ~PromoteDialog() override;

    /// 成るかを尋ねる（成る: true、成らない・閉じた: false）。主画面の盤と詰将棋を解く画面で共通
    static bool ask(QWidget* parent);

private:
    std::unique_ptr<Ui::PromoteDialog> ui;
};

#endif // PROMOTEDIALOG_H
