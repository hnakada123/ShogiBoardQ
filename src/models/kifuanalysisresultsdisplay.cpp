/// @file kifuanalysisresultsdisplay.cpp
/// @brief 棋譜解析結果表示ウィジェットクラスの実装

#include "kifuanalysisresultsdisplay.h"

// 棋譜解析結果を表示するクラス
KifuAnalysisResultsDisplay::KifuAnalysisResultsDisplay(QObject *parent) : QObject(parent) { }

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

QString KifuAnalysisResultsDisplay::displayMove(bool full) const
{
    return cachedDisplay(MoveDisplay, full, m_currentMove, beforeSfen, m_lastUsiMove);
}

QString KifuAnalysisResultsDisplay::displayCandidate(bool full) const
{
    return cachedDisplay(CandidateDisplay, full, m_candidateMove, candidateSfen, candidateUsi);
}

QString KifuAnalysisResultsDisplay::displayPv(bool full) const
{
    return cachedDisplay(PvDisplay, full, m_principalVariation, m_sfen, m_usiPv);
}

QString KifuAnalysisResultsDisplay::cachedDisplay(DisplayKind kind, bool full, const QString& source,
                                                  const QString& sfen, const QString& usi) const
{
    // beforeSfen などは外から直接書き換えられるので、元の値を毎回比べる
    DisplayCache& cache = m_displayCache[kind][full ? 1 : 0];
    const auto style = KifuPresentation::options();
    const int notation = static_cast<int>(style.notation);
    if (cache.notation != notation || cache.alwaysOrigin != style.alwaysOrigin || cache.source != source
        || cache.sfen != sfen || cache.usi != usi) {
        cache.notation = notation;
        cache.alwaysOrigin = style.alwaysOrigin;
        cache.source = source;
        cache.sfen = sfen;
        cache.usi = usi;
        cache.text = kind == PvDisplay ? KifuPresentation::pv(sfen, usi, source, full)
                                       : KifuPresentation::label(source, sfen, usi, full);
    }
    return cache.text;
}
