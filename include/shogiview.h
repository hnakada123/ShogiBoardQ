#ifndef SHOGIVIEW_H
#define SHOGIVIEW_H

#include <algorithm>  // for std::clamp
#include "elidelabel.h"
#include <QIcon>
#include <QMap>
#include <QPointer>
#include <QWidget>

class ShogiBoard;
class SolidToolTip;               // 前方宣言

class ShogiView : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(QSize fieldSize READ fieldSize WRITE setFieldSize NOTIFY fieldSizeChanged)

public:
    class Highlight
    {
    public:
        Highlight() {}
        virtual ~Highlight() {}
        virtual int type() const { return 0; }
    };

    class FieldHighlight : public Highlight
    {
    public:
        enum { Type = 1 };
        FieldHighlight(int file, int rank, QColor color) : m_field(file, rank), m_color(color) {}
        inline int file()  const { return m_field.x(); }
        inline int rank()  const { return m_field.y(); }
        inline QColor color() const { return m_color; }
        int type() const override { return Type; }
    private:
        QPoint m_field;
        QColor m_color;
    };

    explicit ShogiView(QWidget *parent = nullptr);

    void setBoard(ShogiBoard* board);
    ShogiBoard* board() const;

    QSize fieldSize() const;
    QSize sizeHint() const override;

    QRect calculateSquareRectangleBasedOnBoardState(const int file, const int rank) const;
    QRect calculateRectangleForRankOrFileLabel(const int file, const int rank) const;

    void setPiece(char type, const QIcon &icon);
    QIcon piece(QChar type) const;

    QPoint getClickedSquare(const QPoint& clickPosition) const;
    QPoint getClickedSquareInDefaultState(const QPoint& clickPosition) const;
    QPoint getClickedSquareInFlippedState(const QPoint& clickPosition) const;

    void addHighlight(Highlight *hl);
    void removeHighlight(Highlight *hl);
    void removeHighlightAllData();

    inline Highlight *highlight(int index) const { return m_highlights.at(index); }
    inline int highlightCount() const { return m_highlights.size(); }

    // 駒画像のセット（通常/反転）
    void setPieces();
    void setPiecesFlip();

    void setMouseClickMode(bool mouseClickMode);
    int squareSize() const;

    void setPositionEditMode(bool positionEditMode);

    void resetAndEqualizePiecesOnStands();
    void initializeToFlatStartingPosition();
    void shogiProblemInitialPosition();
    void flipBoardSides();

    bool getFlipMode() const;
    void setFlipMode(bool newFlipMode);

    void setErrorOccurred(bool newErrorOccurred);

    void startDrag(const QPoint &from);
    void endDrag();
    bool positionEditMode() const;

    void setBlackClockText(const QString& text);
    void setWhiteClockText(const QString& text);

    QLabel *blackClockLabel() const;
    QLabel *whiteClockLabel() const;

    void setBlackPlayerName(const QString& name);
    void setWhitePlayerName(const QString& name);

    ElideLabel *blackNameLabel() const;
    ElideLabel *whiteNameLabel() const;

    void setStandGapCols(double cols);
    void setNameFontScale(double scale); // 0.2〜1.0 くらい
    void setRankFontScale(double scale);

    // 手番（true=先手手番, false=後手手番）
    void setActiveSide(bool blackTurn);

    // 強調配色を変更したいとき用（任意）
    void setHighlightStyle(const QColor& bgOn, const QColor& fgOn, const QColor& fgOff);

public slots:
    void setFieldSize(QSize fieldSize);
    void enlargeBoard();
    void reduceBoard();

signals:
    void fieldSizeChanged(QSize fieldSize);
    void clicked(const QPoint&);
    void rightClicked(const QPoint&);
    void errorOccurred(const QString& errorMessage);

protected:
    void mouseMoveEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* e) override;
    bool eventFilter(QObject* obj, QEvent* ev) override;

private:
    // 状態
    bool m_errorOccurred{false};
    int  m_squareSize{50};
    int  m_param1{0};
    int  m_param2{0};
    int  m_offsetX{0};
    int  m_offsetY{20};
    bool m_flipMode{false};

    QPointer<ShogiBoard> m_board;
    QSize m_fieldSize;

    QMap<QChar, QIcon> m_pieces;
    QList<Highlight*>  m_highlights;

    bool   m_mouseClickMode{true};
    bool   m_positionEditMode{false};
    bool   m_dragging{false};
    QPoint m_dragFrom;
    QChar  m_dragPiece{' '};
    QPoint m_dragPos;
    bool   m_dragFromStand{false};
    QMap<QChar,int> m_tempPieceStandCounts;

    double m_nameFontScale { 0.36 };

    // 描画イベント
    void paintEvent(QPaintEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

    void drawFile(QPainter* painter, const int file) const;
    void drawRank(QPainter* painter, const int rank) const;
    void drawField(QPainter* painter, const int file, const int rank) const;

    // 駒台のマス描画（既存APIは残し、中で共通関数を使う）
    void drawBlackStandField(QPainter* painter, const int file, const int rank) const;
    void drawWhiteStandField(QPainter* painter, const int file, const int rank) const;

    void drawPiece(QPainter* painter, const int file, const int rank);

    void drawBlackStandPiece(QPainter* painter, const int file, const int rank) const;
    void drawWhiteStandPiece(QPainter* painter, const int file, const int rank) const;
    void drawBlackStandPieceCount(QPainter* painter, const int file, const int rank) const;
    void drawWhiteStandPieceCount(QPainter* painter, const int file, const int rank) const;

    void drawHighlights(QPainter* painter);
    void drawFourStars(QPainter* painter);

    void drawEditModeBlackStandPiece(QPainter* painter, const int file, const int rank) const;
    void drawEditModeBlackStandPieceCount(QPainter* painter, const int file, const int rank) const;
    void drawEditModeWhiteStandPiece(QPainter* painter, const int file, const int rank) const;
    void drawEditModeWhiteStandPieceCount(QPainter* painter, const int file, const int rank) const;

    void drawDraggingPiece(QPainter* painter);

    QChar rankToBlackShogiPiece(const int rank) const;
    QChar rankToWhiteShogiPiece(const int rank) const;

    void drawRanks(QPainter* painter);
    void drawFiles(QPainter* painter);
    void drawBoardFields(QPainter* painter);
    void drawBlackEditModeStand(QPainter* painter);
    void drawWhiteEditModeStand(QPainter* painter);
    void drawEditModeStand(QPainter* painter);
    void drawBlackNormalModeStand(QPainter* painter);
    void drawWhiteNormalModeStand(QPainter* painter);
    void drawNormalModeStand(QPainter* painter);
    void drawEditModeFeatures(QPainter* painter);
    void drawPieces(QPainter* painter);
    void drawPiecesBlackStandInEditMode(QPainter* painter);
    void drawPiecesWhiteStandInEditMode(QPainter* painter);
    void drawPiecesEditModeStandFeatures(QPainter* painter);
    void drawPiecesBlackStandInNormalMode(QPainter* painter);
    void drawPiecesWhiteStandInNormalMode(QPainter* painter);

    // 盤面サイズを更新する共通のロジック（最低限の実装）
    void updateBoardSize();

    // 駒台の駒描画の共通処理
    void drawStandPieceIcon(QPainter* painter, const QRect& adjustedRect, QChar value) const;
    void drawStandPieceCount(QPainter* painter, const QRect& adjustedRect, QChar value) const;

    // ラベル類
    QLabel*    m_blackClockLabel{nullptr};
    QLabel*    m_whiteClockLabel{nullptr};
    ElideLabel* m_blackNameLabel{nullptr};
    ElideLabel* m_whiteNameLabel{nullptr};

    void updateBlackClockLabelGeometry();
    void updateWhiteClockLabelGeometry();
    QRect blackStandBoundingRect() const;
    QRect whiteStandBoundingRect() const;

    void fitLabelFontToRect(QLabel* label, const QString& text,
                            const QRect& rect, int paddingPx = 2);

    void recalcLayoutParams();

    // 駒台と盤の隙間設定
    double m_standGapCols{0.6};
    int    m_standGapPx{0};

    // ラベル帯の厚み・余白・フォントサイズ
    int    m_labelGapPx{8};
    int    m_labelBandPx{36};
    double m_labelFontPt{12.0};

    int boardLeftPx() const;
    int boardRightPx() const;
    int standInnerEdgePx(bool rightSide) const;

    QString m_blackNameBase;   // 記号なしの元名
    QString m_whiteNameBase;   // 記号なしの元名
    void    refreshNameLabels();     // 矢印を付けて表示を更新
    static QString stripMarks(const QString&);

    int    minGapForRankLabelsPx() const;
    double m_rankFontScale{0.8};

    void applyTurnHighlight(bool blackActive);
    QColor m_highlightBg   = QColor(255, 255, 0);
    QColor m_highlightFgOn = QColor(0, 0, 255);
    QColor m_highlightFgOff= QColor(51, 51, 51);
    bool   m_blackActive   = true;

    // 自前ツールチップ
    SolidToolTip* m_tooltip = nullptr;
};

#endif // SHOGIVIEW_H
