#ifndef LEGALMOVESTATUS_H
#define LEGALMOVESTATUS_H

/// @file legalmovestatus.h
/// @brief 合法手存在判定ステータス構造体の定義

/// 指定マスへの合法手の成り/不成の存在状態
struct LegalMoveStatus {
    bool nonPromotingMoveExists;  ///< 不成での合法手が存在するか
    bool promotingMoveExists;     ///< 成りでの合法手が存在するか

    LegalMoveStatus(bool nonPromoting = false, bool promoting = false)
        : nonPromotingMoveExists(nonPromoting), promotingMoveExists(promoting) {}
};

#endif // LEGALMOVESTATUS_H
