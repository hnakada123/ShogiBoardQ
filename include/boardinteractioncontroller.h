#ifndef BOARDINTERACTIONCONTROLLER_H
#define BOARDINTERACTIONCONTROLLER_H

#include <QObject>
#include <QPoint>
#include <QColor>          // ← QColor をヘッダで使うので追加
#include "shogiview.h"     // ← ShogiView::FieldHighlight を使うために必須

class ShogiGameController;

class BoardInteractionController : public QObject
{
    Q_OBJECT
public:
    enum class Mode { HumanVsHuman, HumanVsEngine, Edit };

    explicit BoardInteractionController(ShogiView* view,
                                        ShogiGameController* gc,
                                        QObject* parent = nullptr);

    void setMode(Mode m) { m_mode = m; }
    Mode mode() const { return m_mode; }

public slots:
    void onLeftClick(const QPoint& pt);
    void onRightClick(const QPoint& pt);

    // 呼び元で着手が適用された結果を通知
    void onMoveApplied(const QPoint& from, const QPoint& to, bool success);

    // 局面移動等でハイライトだけ反映したい時
    void showMoveHighlights(const QPoint& from, const QPoint& to);
    void clearAllHighlights();

signals:
    void moveRequested(const QPoint& from, const QPoint& to);
    void selectionChanged(const QPoint& pos);

private:
    // ハイライト系ヘルパ（Main から移植）
    void selectPieceAndHighlight(const QPoint& field);
    void updateHighlight(ShogiView::FieldHighlight*& hl, const QPoint& field, const QColor& color);
    void deleteHighlight(ShogiView::FieldHighlight*& hl);
    void addNewHighlight(ShogiView::FieldHighlight*& hl, const QPoint& pos, const QColor& color);
    void resetSelectionAndHighlight();
    void finalizeDrag();
    void togglePiecePromotionOnClick(const QPoint& field);

    static constexpr int kBlackStandFile = 10;
    static constexpr int kWhiteStandFile = 11;

private:
    ShogiView* m_view = nullptr;
    ShogiGameController* m_gc = nullptr;

    Mode m_mode = Mode::HumanVsHuman;

    // クリック状態
    QPoint m_clickPoint;
    bool   m_waitingSecondClick = false;
    QPoint m_firstClick;

    // ハイライト
    ShogiView::FieldHighlight* m_selectedField  = nullptr; // 選択中（オレンジ）
    ShogiView::FieldHighlight* m_selectedField2 = nullptr; // 直前の移動元（赤）
    ShogiView::FieldHighlight* m_movedField     = nullptr; // 移動先（黄）
};

#endif // BOARDINTERACTIONCONTROLLER_H
