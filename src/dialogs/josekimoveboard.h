#ifndef JOSEKIMOVEBOARD_H
#define JOSEKIMOVEBOARD_H

/// @file josekimoveboard.h
/// @brief 定跡手ダイアログの盤（指した手を指し手・予想応手として知らせる）

#include <QString>
#include <QWidget>

#include "boardinputroute.h"

class BoardInteractionController;
class BoardViewerWidget;
class ShogiView;

/**
 * @brief 定跡手の指し手・予想応手を入力する盤
 *
 * 盤（拡大・縮小・回転つき）と盤の操作をまとめ、指した手を movePlayed() で知らせる。指した手で局面は
 * 進めない。何を表示するかは持ち主が showPosition() で決める。盤の大きさと向きは設定に保存する。
 */
class JosekiMoveBoard : public QWidget, private BoardInputRoute
{
    Q_OBJECT

public:
    explicit JosekiMoveBoard(QWidget* parent = nullptr);

    /**
     * @brief 局面を表示する
     * @param sfen 表示する局面（空なら盤を変えない）
     * @param lastMove その局面に至った手（USI。無ければ空）
     * @param acceptsInput 駒をつまめるか（局面が空なら指せない）
     */
    void showPosition(const QString& sfen, const QString& lastMove, bool acceptsInput);

    ShogiView* view() const;

signals:
    /// 盤で手を指した（USI。合法手だけを知らせる）
    void movePlayed(const QString& usiMove);

private slots:
    /// 拡大・縮小・回転したので、つまんでいた駒を放す
    void cancelSelection();
    void saveSquareSize();
    void saveFlipped(bool flipped);

private:
    // --- BoardInputRoute（盤で指した手） ---
    bool acceptsInput() const override { return m_acceptsInput; }
    void submit(const QPoint& from, const QPoint& to) override;

    BoardViewerWidget* m_viewer = nullptr;
    BoardInteractionController* m_interaction = nullptr;
    bool m_acceptsInput = false;
    QString m_sfen;      ///< 表示している局面
    QString m_lastMove;  ///< 表示している局面に至った手
};

#endif // JOSEKIMOVEBOARD_H
