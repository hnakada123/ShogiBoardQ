#ifndef JOSEKIBOOKMERGEDIALOG_H
#define JOSEKIBOOKMERGEDIALOG_H

/// @file josekibookmergedialog.h
/// @brief 別の定跡ファイルを今の定跡に足すダイアログ（「マージ ▼」→「定跡ファイルから...」）

#include <QDialog>

#include "josekibook.h"

class QCheckBox;
class QLineEdit;
class QPushButton;
class QRadioButton;

/**
 * @brief 足す定跡ファイルと、同じ指し手があったときの扱いを選ぶ
 *
 * 同じ指し手は、今の定跡と足す定跡のどちらの予想応手・評価値・深さ・コメントにするかと、
 * 出現頻度を足し合わせるかを選ぶ。選んだ扱いは保存して次に開いたときに戻す。
 */
class JosekiBookMergeDialog : public QDialog
{
    Q_OBJECT

public:
    /// directory はファイルを選ぶときに初めに開くフォルダ
    explicit JosekiBookMergeDialog(const QString &directory, QWidget *parent = nullptr);
    ~JosekiBookMergeDialog() override;

    QString filePath() const;
    JosekiBook::MergeOptions options() const;

    void done(int result) override;

private slots:
    void browse();
    void updateInputs();

private:
    QString m_directory;
    QLineEdit *m_path = nullptr;
    QRadioButton *m_preferCurrent = nullptr;
    QRadioButton *m_preferIncoming = nullptr;
    QCheckBox *m_sumFrequency = nullptr;
    QPushButton *m_okButton = nullptr;
};

#endif // JOSEKIBOOKMERGEDIALOG_H
