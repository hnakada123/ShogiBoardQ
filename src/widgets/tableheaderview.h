#ifndef TABLEHEADERVIEW_H
#define TABLEHEADERVIEW_H

/// @file tableheaderview.h
/// @brief 見出しの文字の測り直しと、見た目の変わらない描き直しを省く表の見出し

#include <QFont>
#include <QHash>
#include <QHeaderView>
#include <QSize>
#include <QString>

class QStyle;
class QTableView;

/**
 * @brief 見出しの文字の測り直しと、見た目の変わらない描き直しを省く表の見出し
 *
 * - 見出しの文字の大きさ（sectionSizeFromContents）を条件ごとに覚える。QHeaderView は
 *   自分の大きさが変わるたびに見出しの高さの覚えを捨て、すべての見出しの文字を測り直す。
 *   思考表は縦のスクロールバーが出入りするたびに見出しの幅が変わるので、同じ見出しを何度も
 *   測っていた。結果を左右する条件（見出しの文字・書体・アイコン・見出しの大きさ・向き・
 *   有効かどうか・並べ替えの印・スタイル・解像度）が前と同じなら前の結果を返す。スタイル
 *   シートや書体・文字の向きが変わったときは、覚えた結果をすべて捨てる。
 * - 表の選択が変わっても、見出しの描き分けが変わらなければ描き直さない。QHeaderView は
 *   選択に掛かる列と、列全体が選ばれた列を描き分けるので、選択が変わるたびに変わった範囲の
 *   見出しを描き直す。棋譜欄は1手ごとに選ぶ行が替わるが、行単位の選択なので描き分けは
 *   変わらない。描いたときの描き分けを見出しごとに覚え、同じなら描き直さない。
 * - 利用者が見出しの境目をドラッグ（またはダブルクリック）して列幅を変えたときだけ
 *   sectionResizedByUser を出す。列幅を保存する表はこれを受ける。保存した幅の復元・
 *   文字の大きさや内容に合わせた自動の幅・最後の列の伸び縮みでは出ないので、
 *   復元した幅を保存し直したり、自動の幅を利用者の幅と取り違えたりしない。
 */
class TableHeaderView : public QHeaderView
{
    Q_OBJECT

public:
    explicit TableHeaderView(Qt::Orientation orientation, QWidget* parent = nullptr);

    /// view の横の見出しをこのクラスに替える（QTableView の既定の見出しと同じ設定にする）
    static TableHeaderView* install(QTableView* view);

    /// 見出しの大きさを決める条件
    struct SizeKey {
        int section = -1;
        QString text;
        QFont font;
        qint64 icon = 0;
        QSize headerSize;
        bool enabled = false;
        Qt::LayoutDirection direction = Qt::LeftToRight;
        bool sortIndicator = false;
        const QStyle* style = nullptr;
        int dpi = 0;
        bool operator==(const SizeKey& other) const;
    };

signals:
    /// 利用者の操作で列幅が変わった
    void sectionResizedByUser(int logicalIndex, int oldSize, int newSize);

protected:
    QSize sectionSizeFromContents(int logicalIndex) const override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    bool event(QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void selectionChanged(const QItemSelection& selected, const QItemSelection& deselected) override;

private slots:
    void onSectionResized(int logicalIndex, int oldSize, int newSize);

private:
    /// 選択による見出しの描き分け（QHeaderView::initStyleOptionForIndex の State_On と State_Sunken）
    int selectionState(int logicalIndex) const;
    QRect sectionRect(int logicalIndex) const;

    static constexpr qsizetype kMaxCachedSizes = 256;
    mutable QHash<SizeKey, QSize> m_sizes;
    QHash<int, int> m_paintedSelectionStates;  ///< 見出しを最後に描いたときの描き分け
    bool m_mouseOperating = false;             ///< 見出しをマウスで操作している間
};

size_t qHash(const TableHeaderView::SizeKey& key, size_t seed = 0);

#endif // TABLEHEADERVIEW_H
