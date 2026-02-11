#ifndef KIFUPASTEDIALOG_H
#define KIFUPASTEDIALOG_H

/// @file kifupastedialog.h
/// @brief 棋譜貼り付けダイアログクラスの定義


#include <QDialog>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QCloseEvent>

/**
 * @brief 棋譜貼り付けダイアログ
 * 
 * クリップボードからコピーした棋譜テキストを貼り付けて取り込むためのダイアログ。
 * KIF、KI2、CSA、USI、JKF、USEN形式を自動判定して読み込む。
 */
class KifuPasteDialog : public QDialog
{
    Q_OBJECT

public:
    explicit KifuPasteDialog(QWidget* parent = nullptr);
    ~KifuPasteDialog() override;

protected:
    void closeEvent(QCloseEvent* event) override;

    /**
     * @brief 入力されたテキストを取得
     * @return ユーザーが入力/貼り付けたテキスト
     */
    QString text() const;

signals:
    /**
     * @brief 「取り込む」ボタンが押されたときに発行
     * @param content 入力されたテキスト内容
     */
    void importRequested(const QString& content);

private slots:
    void onImportClicked();
    void onCancelClicked();
    void onPasteClicked();
    void onClearClicked();

private:
    void setupUi();

    QPlainTextEdit* m_textEdit = nullptr;
    QPushButton* m_btnImport = nullptr;
    QPushButton* m_btnCancel = nullptr;
    QPushButton* m_btnPaste = nullptr;
    QPushButton* m_btnClear = nullptr;
    QLabel* m_lblInfo = nullptr;
};

#endif // KIFUPASTEDIALOG_H
