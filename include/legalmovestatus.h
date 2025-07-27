#ifndef LEGALMOVESTATUS_H
#define LEGALMOVESTATUS_H

struct LegalMoveStatus {
    bool nonPromotingMoveExists;  // 成らない状態での合法手が存在するかどうか
    bool promotingMoveExists;     // 成る状態での合法手が存在するかどうか

    // コンストラクタ
    LegalMoveStatus(bool nonPromoting = false, bool promoting = false)
        : nonPromotingMoveExists(nonPromoting), promotingMoveExists(promoting) {}
};

#endif // LEGALMOVESTATUS_H
