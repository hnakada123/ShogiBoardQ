#ifndef KIFUANALYSISRESULTSDISPLAY_H
#define KIFUANALYSISRESULTSDISPLAY_H

#include <QObject>
#include <QString>

class KifuAnalysisResultsDisplay : public QObject
{
    Q_OBJECT
public:
    // コンストラクタ
    explicit KifuAnalysisResultsDisplay(QObject *parent = nullptr);

    // コンストラクタ
    KifuAnalysisResultsDisplay(const QString& currentMove, const QString& evaluationValue, const QString& evaluationDifference,
                               const QString& principalVariation, QObject* parent = nullptr);

private:
    // 指し手
    QString m_currentMove;

    // 評価値
    QString m_evaluationValue;

    // 評価値差
    QString m_evaluationDifference;

    // 読み筋
    QString m_principalVariation;

public:
    // 指し手を取得する。
    QString currentMove() const;

    // 評価値を取得する。
    QString evaluationValue() const;

    // 評価値差を取得する。
    QString evaluationDifference() const;

    // 読み筋を取得する。
    QString principalVariation() const;
};

#endif // KIFUANALYSISRESULTSDISPLAY_H
