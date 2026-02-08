#ifndef BRANCHROWDELEGATE_H
#define BRANCHROWDELEGATE_H

/// @file branchrowdelegate.h
/// @brief 分岐行描画デリゲートクラスの定義


#include <QStyledItemDelegate>
#include <QSet>

/**
 * @brief 分岐行の表示をカスタマイズするデリゲートクラス
 *
 * 責務:
 * - 棋譜表示の分岐マーカー（●）を描画する
 * - 選択行の背景色をカスタマイズする
 *
 * 元々MainWindowの内部クラスとして定義されていたが、
 * 責務分離のため独立したクラスに移動。
 */
class BranchRowDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    /**
     * @brief コンストラクタ
     * @param parent 親オブジェクト
     */
    explicit BranchRowDelegate(QObject* parent = nullptr);

    /**
     * @brief デストラクタ
     */
    ~BranchRowDelegate() override = default;

    /**
     * @brief 分岐マーカーを設定
     * @param marks 分岐のある手数（ply）のセット
     */
    void setMarkers(const QSet<int>* marks) { m_marks = marks; }

    /**
     * @brief 描画処理のオーバーライド
     * @param painter 描画オブジェクト
     * @param option 描画オプション
     * @param index モデルインデックス
     */
    void paint(QPainter* painter,
               const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;

private:
    const QSet<int>* m_marks = nullptr;
};

#endif // BRANCHROWDELEGATE_H
