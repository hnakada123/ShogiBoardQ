#ifndef USIMESSAGES_H
#define USIMESSAGES_H

/// @file usimessages.h
/// @brief エンジンから受け取る USI のメッセージ（option・bestmove・checkmate）の値型

#include <QMetaType>
#include <QString>
#include <QStringList>

#include "shogitypes.h"
#include "usiscore.h"

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
