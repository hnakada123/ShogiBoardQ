#ifndef BOARDSYNCPRESENTER_H
#define BOARDSYNCPRESENTER_H

/// @file boardsyncpresenter.h
/// @brief 盤面同期プレゼンタクラスの定義


#include <QObject>
#include <QStringList>
#include <QList>
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
        const QList<ShogiMove>* gameMoves = nullptr;  // 参照（外部所有）
    };

    explicit BoardSyncPresenter(const Deps& deps, QObject* parent=nullptr);

    // SFEN を適用して盤を描画
    void applySfenAtPly(int ply) const;

    // 盤描画＋最終手のハイライトまで一括適用（ply=0 はハイライト無し）
    void syncBoardAndHighlightsAtRow(int ply) const;

    // ユーティリティ：ハイライトの明示クリア
    void clearHighlights() const;

    /// sfenRecord ポインタを更新する（MatchCoordinator 再生成時に呼ぶ）
    void setSfenRecord(const QStringList* sfenRecord) { m_sfenHistory = sfenRecord; }

    /**
     * @brief SFEN差分から盤面を更新しハイライトを表示（分岐ナビゲーション用）
     * @param currentSfen 現在の局面SFEN
     * @param prevSfen 直前の局面SFEN（空の場合はハイライトクリア）
     *
     * m_sfenHistoryを使わず、指定されたSFEN文字列から直接盤面を更新し、
     * 差分からハイライトを計算・表示する。
     */
    void loadBoardWithHighlights(const QString& currentSfen, const QString& prevSfen) const;

private:
    static QPoint toOne(const QPoint& z) { return QPoint(z.x() + 1, z.y() + 1); }

    ShogiGameController* m_gc;
    ShogiView* m_view;
    BoardInteractionController* m_bic;
    const QStringList* m_sfenHistory;
    const QList<ShogiMove>* m_gameMoves;
};

#endif // BOARDSYNCPRESENTER_H
