#ifndef BOARDARROWACTIONS_H
#define BOARDARROWACTIONS_H

/// @file boardarrowactions.h
/// @brief 盤の矢印を出すかの切り替え（checkable な QAction の組）と、その保存

#include <QObject>

class QAction;

/**
 * @brief 盤の矢印を出すかの4つの切り替え
 *
 * どれも checkable な QAction で、これが正本になる。起動時に設定から戻し、変わるたびに保存する。
 * - 思考ドックの「対局中の矢印表示」「先読み側も表示」、検討タブの「矢印表示」、定跡ウィンドウの「盤に矢印」の
 *   チェックは、渡された QAction に結ぶだけ
 * - 盤の矢印（CandidateArrowPresenter）はこの QAction だけを読む
 */
class BoardArrowActions : public QObject
{
    Q_OBJECT

public:
    /// joseki は「表示」メニューの「盤に定跡手を表示」（非所有。無ければここが作る）。ほかの3つはここが作る
    explicit BoardArrowActions(QAction* joseki, QObject* parent = nullptr);

    /// 対局のエンジンの手番側の読み（赤の実線。思考ドックの「対局中の矢印表示」）
    QAction* match() const { return m_match; }
    /// 対局のエンジンの先読み側の読み（青の破線。思考ドックの「先読み側も表示」。match がオンのときだけ出す）
    QAction* ponder() const { return m_ponder; }
    /// 検討の候補手（検討タブの「矢印表示」）
    QAction* consideration() const { return m_consideration; }
    /// 定跡ウィンドウで開いている定跡の定跡手（緑。「表示」メニューの「盤に定跡手を表示」）
    QAction* joseki() const { return m_joseki; }

private slots:
    /// 4つの切り替えを保存する
    void save();

private:
    QAction* m_match = nullptr;
    QAction* m_ponder = nullptr;
    QAction* m_consideration = nullptr;
    QAction* m_joseki = nullptr;
};

#endif // BOARDARROWACTIONS_H
