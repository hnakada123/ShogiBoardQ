#ifndef USIMESSAGES_H
#define USIMESSAGES_H

/// @file usimessages.h
/// @brief USI のメッセージの値型（受信の option・bestmove・checkmate と、送信の gameover の語）

#include <QMetaType>
#include <QString>
#include <QStringList>

#include "shogitypes.h"
#include "usiscore.h"

/// bestmove の特殊手を表す列挙型（エンジンからの受信用）
enum class SpecialMove { None, Resign, Win, Draw };

/// bestmove 文字列を SpecialMove に変換する（通常手は None を返す）
inline SpecialMove parseSpecialMove(const QString& s)
{
    if (s.compare(QStringLiteral("resign"), Qt::CaseInsensitive) == 0) return SpecialMove::Resign;
    if (s.compare(QStringLiteral("win"),    Qt::CaseInsensitive) == 0) return SpecialMove::Win;
    if (s.compare(QStringLiteral("draw"),   Qt::CaseInsensitive) == 0) return SpecialMove::Draw;
    return SpecialMove::None;
}

/// gameover コマンドの結果を表す列挙型（エンジンへの送信用）
enum class GameOverResult { Win, Lose, Draw };

/// GameOverResult を USI gameover コマンド用の文字列に変換する
inline QString gameOverResultToString(GameOverResult r)
{
    switch (r) {
    case GameOverResult::Win:  return QStringLiteral("win");
    case GameOverResult::Lose: return QStringLiteral("lose");
    case GameOverResult::Draw: return QStringLiteral("draw");
    }
    return QStringLiteral("draw");
}

/**
 * @brief bestmove（探索の結果）
 *
 * 解析器は move と ponder だけを入れる。探索の文脈（経過時間・最後の評価値・先読みだったか・
 * stop の後か）は UsiProtocolHandler が入れる。
 */
struct UsiBestMove {
    QString move;                          ///< 指し手（USI 形式）か resign / win / draw
    QString ponder;                        ///< 予想する相手の応手（無ければ空）
    QString baseSfen;                      ///< 探索した局面
    SpecialMove special = SpecialMove::None;
    qint64 elapsedMs = 0;                  ///< go（先読みが当たったときは ponderhit）から bestmove まで
    UsiScore score;                        ///< この探索の最善の読み筋（multipv 1）の最後の評価値（手番側から見た値）
    bool pondering = false;                ///< 先読み（go ponder）の結果。外れて stop で回収したもの
    bool stopped = false;                  ///< stop を送った後の結果

    bool isMove() const { return special == SpecialMove::None; }
};

/// checkmate（go mate の結果）
struct UsiCheckmate {
    enum class Result { Mate, NoMate, NotImplemented, Unknown };
    Result result = Result::Unknown;
    QStringList pv;                        ///< Mate のときの詰み手順（USI 形式）
    bool stopped = false;                  ///< stop を送った後の結果（打ち切られた探索の手順かもしれない）
};

Q_DECLARE_METATYPE(UsiBestMove)
Q_DECLARE_METATYPE(UsiCheckmate)

#endif // USIMESSAGES_H
