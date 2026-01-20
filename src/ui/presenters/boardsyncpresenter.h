#ifndef BOARDSYNCPRESENTER_H
#define BOARDSYNCPRESENTER_H

#include <QObject>
#include <QStringList>
#include <QVector>
#include <QPoint>

class ShogiGameController;
class ShogiView;
class BoardInteractionController;
struct ShogiMove;

class BoardSyncPresenter : public QObject
{
    Q_OBJECT
public:
    struct Deps {
        ShogiGameController* gc = nullptr;
        ShogiView* view = nullptr;
        BoardInteractionController* bic = nullptr;
        const QStringList* sfenRecord = nullptr;        // 参照（外部所有）
        const QVector<ShogiMove>* gameMoves = nullptr;  // 参照（外部所有）
    };

    explicit BoardSyncPresenter(const Deps& deps, QObject* parent=nullptr);

    // SFEN を適用して盤を描画
    void applySfenAtPly(int ply) const;

    // 盤描画＋最終手のハイライトまで一括適用（ply=0 はハイライト無し）
    void syncBoardAndHighlightsAtRow(int ply) const;

    // ユーティリティ：ハイライトの明示クリア
    void clearHighlights() const;

private:
    static QPoint toOne(const QPoint& z) { return QPoint(z.x() + 1, z.y() + 1); }

    ShogiGameController* m_gc;
    ShogiView* m_view;
    BoardInteractionController* m_bic;
    const QStringList* m_sfenRecord;
    const QVector<ShogiMove>* m_gameMoves;
};

#endif // BOARDSYNCPRESENTER_H
