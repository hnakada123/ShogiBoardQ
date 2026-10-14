#ifndef PROMOTIONFLOW_H
#define PROMOTIONFLOW_H

/// @file promotionflow.h
/// @brief 成り・不成の選択フローを提供するユーティリティクラス


class QWidget;

/**
 * @brief 駒の成り・不成をユーザーに問い合わせるフロー
 */
class PromotionFlow final {
public:
    /// @return 成る: true / 不成: false
    static bool askPromote(QWidget* parent);
};

#endif // PROMOTIONFLOW_H
