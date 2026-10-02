#ifndef JISHOGISCOREDIALOG_H
#define JISHOGISCOREDIALOG_H

#include <QDialog>
#include <QStringList>

#include "fontsizehelper.h"
#include "jishogicalculator.h"

class QGridLayout;
class QHBoxLayout;
class QLabel;
class QPushButton;

/// 表示局面の持将棋点数と入玉宣言条件を先後比較で表示する。
class JishogiScoreDialog : public QDialog
{
    Q_OBJECT

public:
    explicit JishogiScoreDialog(const JishogiCalculator::JishogiResult& result,
                               bool senteInCheck, bool goteInCheck, QWidget* parent = nullptr);
    ~JishogiScoreDialog() override;

private slots:
    void decreaseFontSize();
    void increaseFontSize();
    void copyReport();

private:
    QHBoxLayout* createButtons();
    void addRow(QGridLayout* grid, const QString& title, const QString& sente,
                const QString& gote, const QString& name, bool emphasis = false);
    void applyFontSize();
    static QString conditionText(bool met);
    static QString ruleText(const JishogiCalculator::PlayerScore& score,
                            bool inCheck, bool isSente, bool rule24);

    QWidget* m_content = nullptr;
    QPushButton* m_fontDecrease = nullptr;
    QPushButton* m_fontIncrease = nullptr;
    QLabel* m_fontSizeLabel = nullptr;
    QStringList m_report;
    FontSizeHelper m_fontHelper;
};

#endif // JISHOGISCOREDIALOG_H
