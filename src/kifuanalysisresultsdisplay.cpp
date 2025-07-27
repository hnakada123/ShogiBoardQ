#include "kifuanalysisresultsdisplay.h"

// 棋譜解析結果を表示するクラス
// コンストラクタ
KifuAnalysisResultsDisplay::KifuAnalysisResultsDisplay(QObject *parent) : QObject(parent) { }

// コンストラクタ
KifuAnalysisResultsDisplay::KifuAnalysisResultsDisplay(const QString &currentMove, const QString &evaluationValue,
                                     const QString &evaluationDifference, const QString &principalVariation, QObject *parent)
    : KifuAnalysisResultsDisplay(parent)
{
    // 指し手、評価値、差、読み筋を設定する。
    m_currentMove = currentMove;
    m_evaluationValue = evaluationValue;
    m_evaluationDifference = evaluationDifference;
    m_principalVariation = principalVariation;
}

// 読み筋を取得する。
QString KifuAnalysisResultsDisplay::principalVariation() const
{
    return m_principalVariation;
}

// 評価値差を取得する。
QString KifuAnalysisResultsDisplay::evaluationDifference() const
{
    return m_evaluationDifference;
}

// 評価値を取得する。
QString KifuAnalysisResultsDisplay::evaluationValue() const
{
    return m_evaluationValue;
}

// 指し手を取得する。
QString KifuAnalysisResultsDisplay::currentMove() const
{
    return m_currentMove;
}
