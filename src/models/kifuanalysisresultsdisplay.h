#ifndef KIFUANALYSISRESULTSDISPLAY_H
#define KIFUANALYSISRESULTSDISPLAY_H

/// @file kifuanalysisresultsdisplay.h
/// @brief 棋譜解析の結果の表の1行

#include <QObject>
#include <QString>
#include <optional>

#include "kifupresentation.h"
#include "usiscore.h"

/**
 * @brief 棋譜解析の結果の表の1行（1局面の解析結果）
 *
 * 評価値は先手から見た値（UsiScore）で持つ。表示する文字列（「mate 3」・形勢）は表のモデルが作る。
 */
class KifuAnalysisResultsDisplay : public QObject
{
    Q_OBJECT
public:
    explicit KifuAnalysisResultsDisplay(QObject *parent = nullptr);

    /**
     * @param evaluation 先手から見た評価値（定跡・未取得は None）
     * @param evaluationDifference 直前の解析局面からの評価値の増減（比べられなければ nullopt）
     */
    KifuAnalysisResultsDisplay(const QString& currentMove, const UsiScore& evaluation,
                               std::optional<int> evaluationDifference, const QString& principalVariation,
                               QObject* parent = nullptr);

private:
    // 指し手
    QString m_currentMove;

    // 候補手（前の行の読み筋の最初の指し手）
    QString m_candidateMove;

    UsiScore m_evaluation;
    std::optional<int> m_evaluationDifference;

    // 読み筋（漢字表記）
    QString m_principalVariation;

    // 読み筋（USI形式）
    QString m_usiPv;

    // 局面SFEN
    QString m_sfen;

public:
    QString beforeSfen;
    QString candidateUsi;
    QString candidateSfen;
    // 表示用の表記。表の描画と列幅の計算のたびに呼ばれる（英語表記では局面をたどって作るので重い）。
    // 元の値と表記の設定が前と同じなら、前に作った文字列を返す
    QString displayMove(bool full = false) const;
    QString displayCandidate(bool full = false) const;
    QString displayPv(bool full = false) const;

    // 指し手を取得する。
    QString currentMove() const;

    // 候補手を取得する。
    QString candidateMove() const { return m_candidateMove; }

    // 候補手を設定する。
    void setCandidateMove(const QString& move) { m_candidateMove = move; }

    /// 先手から見た評価値（定跡・未取得は None）
    const UsiScore& evaluation() const { return m_evaluation; }
    /// 直前の解析局面からの評価値の増減（比べられなければ nullopt）
    std::optional<int> evaluationDifference() const { return m_evaluationDifference; }

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

    /// 表示用の表記と、それを作ったときの元の値
    struct DisplayCache {
        QString source;
        QString sfen;
        QString usi;
        int notation = -1;
        bool alwaysOrigin = false;
        QString text;
    };
    enum DisplayKind { MoveDisplay, CandidateDisplay, PvDisplay, DisplayKindCount };
    /// [種類][移動元をすべて付けるか]
    mutable DisplayCache m_displayCache[DisplayKindCount][2];

    QString cachedDisplay(DisplayKind kind, bool full, const QString& source, const QString& sfen,
                          const QString& usi) const;
};

#endif // KIFUANALYSISRESULTSDISPLAY_H
