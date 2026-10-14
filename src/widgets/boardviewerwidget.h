#ifndef BOARDVIEWERWIDGET_H
#define BOARDVIEWERWIDGET_H

/// @file boardviewerwidget.h
/// @brief 盤（ShogiBoard + ShogiView）と、拡大・縮小・回転・局面送りの操作をまとめた部品

#include <QStringList>
#include <QWidget>

class ShogiBoard;
class ShogiView;
class QHBoxLayout;
class QLabel;
class QPushButton;
class QScrollArea;

/**
 * @brief 読み筋・局面集・詰将棋・外観の見本で共通に使う盤の表示部品
 *
 * 次の部品を持つ（Features で選ぶ）。
 * - 盤の上の操作列: 縮小・拡大・盤面の回転（toolbarLayout() に呼び出し側のボタンを足せる）
 * - 盤: ShogiView（Ctrl+ホイールで拡大・縮小）。ScrollArea を選ぶと枠なしのスクロール領域に入れる
 * - 盤の下: 局面の説明ラベル（setPositionText）と、最初・前・次・最後の局面送り
 *
 * 局面送りは setSfenList() で渡した局面の一覧を currentIndex() で表示する。表示を替えると
 * currentIndexChanged() を出す。盤は変わったマスだけを描き直すので、全体の描き直しは頼まない。
 */
class BoardViewerWidget : public QWidget
{
    Q_OBJECT

public:
    enum Feature {
        NoFeatures = 0,
        ZoomToolbar = 0x1,   ///< 縮小・拡大・回転の操作列
        Navigation = 0x2,    ///< 局面の説明と、最初・前・次・最後の局面送り
        ScrollArea = 0x4,    ///< 盤をスクロール領域に入れ、拡大・縮小でウィンドウを盤に合わせる
        FitWindowOnZoom = 0x8,  ///< 拡大・縮小のたびにウィンドウを盤に合わせる
    };
    Q_DECLARE_FLAGS(Features, Feature)

    /// ボタンの表記。記号（－＋ と ◀▶）か、言葉（縮小・拡大と 最初・前へ…）
    enum class ButtonLabels { Symbols, Words };

    /// @param externalBoard 盤のモデルを外から渡すとき（対局の盤を表示する詰将棋など）。nullptr なら自分で持つ
    explicit BoardViewerWidget(Features features, ButtonLabels labels = ButtonLabels::Symbols,
                               ShogiBoard* externalBoard = nullptr, QWidget* parent = nullptr);

    ShogiView* view() const { return m_view; }
    ShogiBoard* board() const { return m_board; }

    /// 盤の上の操作列（ZoomToolbar のときだけ）。呼び出し側のボタンを右端に足すのに使う
    QHBoxLayout* toolbarLayout() const { return m_toolbar; }
    QPushButton* reduceButton() const { return m_reduce; }
    QPushButton* enlargeButton() const { return m_enlarge; }
    QPushButton* flipButton() const { return m_flip; }

    /// 局面送りのボタンの説明（ツールチップと読み上げ名）
    void setNavigationToolTips(const QString& first, const QString& back,
                               const QString& forward, const QString& last);

    /// 表示する局面の一覧。表示中の位置は先頭に戻す（空なら盤は替えない）
    void setSfenList(const QStringList& sfens);
    const QStringList& sfenList() const { return m_sfens; }
    int count() const { return static_cast<int>(m_sfens.size()); }
    int currentIndex() const { return m_index; }
    QString currentSfen() const;

    /// 局面の説明（Navigation のときだけ表示する）
    void setPositionText(const QString& text);

    /// 盤の向き（回転ボタンを押したときは flipToggled() も出す）
    void setFlipped(bool flipped);
    bool isFlipped() const;

    /// 盤の大きさを確定させ、時計を隠す。FitWindowOnZoom ならウィンドウも盤に合わせる
    void fitToBoard();

public slots:
    void setCurrentIndex(int index);
    void goFirst();
    void goBack();
    void goForward();
    void goLast();
    void enlargeBoard();
    void reduceBoard();
    void toggleFlip();

signals:
    /// 表示する局面を替えた（setSfenList・局面送り）
    void currentIndexChanged(int index);
    /// 拡大・縮小・回転の直前（盤の選択やドラッグを取り消すため）
    void boardTransformAboutToChange();
    /// 回転ボタンで向きを変えた
    void flipToggled(bool flipped);
    /// 拡大・縮小で盤の大きさが変わった
    void boardResized();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void buildToolbar(ButtonLabels labels);
    void buildNavigation(ButtonLabels labels);
    void showCurrent();
    void updateNavigationButtons();
    void zoom(bool enlarge);

    Features m_features;
    ShogiBoard* m_board = nullptr;
    ShogiView* m_view = nullptr;
    QScrollArea* m_scroll = nullptr;
    QHBoxLayout* m_toolbar = nullptr;
    QPushButton* m_reduce = nullptr;
    QPushButton* m_enlarge = nullptr;
    QPushButton* m_flip = nullptr;
    QLabel* m_positionLabel = nullptr;
    QPushButton* m_first = nullptr;
    QPushButton* m_back = nullptr;
    QPushButton* m_forward = nullptr;
    QPushButton* m_last = nullptr;
    QStringList m_sfens;
    int m_index = 0;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(BoardViewerWidget::Features)

#endif // BOARDVIEWERWIDGET_H
