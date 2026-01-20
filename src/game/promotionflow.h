#ifndef PROMOTIONFLOW_H
#define PROMOTIONFLOW_H

class QWidget;

class PromotionFlow final {
public:
    // 成る: true / 不成: false を返す
    static bool askPromote(QWidget* parent);
};

#endif // PROMOTIONFLOW_H
