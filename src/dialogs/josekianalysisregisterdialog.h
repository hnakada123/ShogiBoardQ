#ifndef JOSEKIANALYSISREGISTERDIALOG_H
#define JOSEKIANALYSISREGISTERDIALOG_H

/// @file josekianalysisregisterdialog.h
/// @brief 棋譜解析の結果をまとめて定跡に登録する範囲のダイアログ（結果の表の右クリック「結果をまとめて定跡に登録...」）

#include <QDialog>

class QCheckBox;
class QPushButton;
class QSpinBox;

/**
 * @brief 定跡に登録する局面の手数の範囲と手番を選ぶダイアログ
 *
 * 手番の選択は保存し、次に開いたときに戻す（手数の範囲は解析した手順で決まるので保存しない）。
 */
class JosekiAnalysisRegisterDialog : public QDialog
{
    Q_OBJECT

public:
    /// 登録する局面の範囲
    struct Range {
        int fromPly = 0;
        int toPly = 0;
        bool black = true;  ///< 先手の手番の局面
        bool white = true;  ///< 後手の手番の局面
    };

    /// minPly・maxPly は解析した局面の手数の範囲（初めはその全体を選ぶ）
    JosekiAnalysisRegisterDialog(int minPly, int maxPly, QWidget *parent = nullptr);
    ~JosekiAnalysisRegisterDialog() override;

    Range range() const;

    void done(int result) override;

private slots:
    /// 範囲の終わりが始めより前にならないようにし、手番を1つも選んでいなければ「登録」を押せなくする
    void updateInputs();

private:
    QSpinBox *m_fromPly = nullptr;
    QSpinBox *m_toPly = nullptr;
    QCheckBox *m_black = nullptr;
    QCheckBox *m_white = nullptr;
    QPushButton *m_okButton = nullptr;
};

#endif // JOSEKIANALYSISREGISTERDIALOG_H
