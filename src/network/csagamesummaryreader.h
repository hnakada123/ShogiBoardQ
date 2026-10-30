#ifndef CSAGAMESUMMARYREADER_H
#define CSAGAMESUMMARYREADER_H

/// @file csagamesummaryreader.h
/// @brief CSA の対局条件（Game_Summary）の読み取り

#include <QString>

#include "csagamesummary.h"

/**
 * @brief Game_Summary を1行ずつ読む（ソケットや接続の状態は持たない）
 *
 * "BEGIN Game_Summary" の次の行から "END Game_Summary" までを read() に渡す。
 * 読み終えたら Finished を返し、summary() が対局条件。行の数か大きさが上限を超えたら TooLarge を返す
 * （サーバーが送る大きな対局条件で、メモリを使い果たさない）。
 */
class CsaGameSummaryReader
{
public:
    /// 読み取りの状態
    enum class Status {
        Reading,    ///< 続きを待つ
        Finished,   ///< "END Game_Summary" まで読んだ
        TooLarge,   ///< 行の数か大きさが上限を超えた
    };

    static constexpr qsizetype kMaxBytes = 2 * 1024 * 1024;
    static constexpr int kMaxLines = 20000;

    /// 1行を読む（Finished・TooLarge の後は読まない）
    Status read(const QString& line);
    /// 読んだ対局条件
    const CsaGameSummary& summary() const { return m_summary; }

private:
    /// Game_Summary のどの階層を読んでいるか
    enum class Section {
        TopLevel,   ///< Game_Summary の直下
        Time,       ///< BEGIN Time（双方の持時間）
        TimeBlack,  ///< BEGIN Time+（先手の持時間）
        TimeWhite,  ///< BEGIN Time-（後手の持時間）
        Position,   ///< BEGIN Position
    };

    /// 階層の始まり・終わりの行なら読んで true
    bool readSectionBoundary(const QString& line);
    /// 持時間の階層の行（Time_Unit:1sec など）
    void readTimeLine(const QString& line);
    /// 局面の階層の行（P1〜P9・P+・P-・手番、再開局面の指し手）
    void readPositionLine(const QString& line);
    /// 直下の行（Game_ID:... など）
    void readTopLevelLine(const QString& line);

    Section m_section = Section::TopLevel;
    CsaGameSummary m_summary;
    Status m_status = Status::Reading;
    qsizetype m_bytes = 0;
    int m_lines = 0;
};

#endif // CSAGAMESUMMARYREADER_H
