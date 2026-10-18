#ifndef JISHOGISCOREDIALOG_H
#define JISHOGISCOREDIALOG_H

#include <QDialog>
#include <QStringList>

#include "jishogicalculator.h"

class QGridLayout;
class QHBoxLayout;
class QLabel;
class QPushButton;

/// 表示局面の持将棋点数と入玉宣言条件を先後比較で表示する。
class Position;

class JishogiScoreDialog : public QDialog
{
    Q_OBJECT

public:
    explicit JishogiScoreDialog(const JishogiCalculator::JishogiResult& result,
                               bool senteInCheck, bool goteInCheck, QWidget* parent = nullptr);
    ~JishogiScoreDialog() override;

    /// position の持将棋の点数と王手の有無を計算して、モーダルで表示する
    static void showFor(const Position& position, QWidget* parent);

private slots:
    void copyReport();
    /// 文字の大きさに合わせて、値の欄を折り返さない幅にする
    void fitValueCells();

private:
    QHBoxLayout* createButtons();
    void addRow(QGridLayout* grid, const QString& title, const QString& sente,
                const QString& gote, const QString& name, bool emphasis = false);
    static QString conditionText(bool met);
    static QString ruleText(const JishogiCalculator::PlayerScore& score,
                            bool inCheck, bool isSente, bool rule24);

    QWidget* m_content = nullptr;
    QPushButton* m_fontDecrease = nullptr;
    QPushButton* m_fontIncrease = nullptr;
    QLabel* m_fontSizeLabel = nullptr;
    QStringList m_report;
};

#endif // JISHOGISCOREDIALOG_H
