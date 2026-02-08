#ifndef KIFUANALYSISRESULTSDISPLAY_H
#define KIFUANALYSISRESULTSDISPLAY_H

/// @file kifuanalysisresultsdisplay.h
/// @brief 棋譜解析結果表示ウィジェットクラスの定義
/// @todo remove コメントスタイルガイド適用済み


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

    // 候補手（前の行の読み筋の最初の指し手）
    QString m_candidateMove;

    // 評価値
    QString m_evaluationValue;

    // 評価値差
    QString m_evaluationDifference;

    // 読み筋（漢字表記）
    QString m_principalVariation;

    // 読み筋（USI形式）
    QString m_usiPv;

    // 局面SFEN
    QString m_sfen;

public:
    // 指し手を取得する。
    QString currentMove() const;

    // 候補手を取得する。
    QString candidateMove() const { return m_candidateMove; }

    // 候補手を設定する。
    void setCandidateMove(const QString& move) { m_candidateMove = move; }

    // 評価値を取得する。
    QString evaluationValue() const;

    // 評価値差を取得する。
    QString evaluationDifference() const;

    // 読み筋（漢字表記）を取得する。
    QString principalVariation() const;

    // 読み筋（USI形式）を取得する。
    QString usiPv() const { return m_usiPv; }

    // 読み筋（USI形式）を設定する。
    void setUsiPv(const QString& usiPv) { m_usiPv = usiPv; }

    // 局面SFENを取得する。
    QString sfen() const { return m_sfen; }

    // 局面SFENを設定する。
    void setSfen(const QString& sfen) { m_sfen = sfen; }

    // 最後の指し手（USI形式）を取得する。
    QString lastUsiMove() const { return m_lastUsiMove; }

    // 最後の指し手（USI形式）を設定する。
    void setLastUsiMove(const QString& move) { m_lastUsiMove = move; }

private:
    // 最後の指し手（USI形式）- 読み筋表示ウィンドウのハイライト用
    QString m_lastUsiMove;
};

#endif // KIFUANALYSISRESULTSDISPLAY_H
