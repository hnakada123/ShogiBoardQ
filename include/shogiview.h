#ifndef SHOGIVIEW_H
#define SHOGIVIEW_H

#include "elidelabel.h"
#include <QIcon>
#include <QMap>
#include <QPointer>
#include <QWidget>

class ShogiBoard;

class ShogiView : public QWidget
{
    // Q_OBJECTマクロ: このクラスがQtのオブジェクトシステムに組み込まれることを示す。
    // シグナルやスロット、プロパティなどのQt特有の機能を使用可能にするために必要
    Q_OBJECT

    // Q_PROPERTYマクロ: クラスのプロパティを定義する。
    // 「fieldSize」という名前のQSize型のプロパティを定義する。
    // READ fieldSize: このプロパティの値を読み出すためのメンバ関数（fieldSize()）を指定する。
    // WRITE setFieldSize: このプロパティの値を設定するためのメンバ関数（setFieldSize()）を指定する。
    // NOTIFY fieldSizeChanged: このプロパティの値が変更された時に発信されるシグナル（fieldSizeChanged()）を指定する。
    // これにより、プロパティの値が変更されると自動的に関連するUI部品などが更新されるようになる。
    Q_PROPERTY(QSize fieldSize READ fieldSize WRITE setFieldSize NOTIFY fieldSizeChanged)

public:
    // 将棋盤上の特定のマスや駒などをハイライト表示するための基底クラス
    // このクラスは、ハイライトの種類を示すための基本的なインターフェースを提供する。
    class Highlight
    {
    public:
        // コンストラクタ
        Highlight() { }

        // バーチャルデストラクタ
        virtual ~Highlight() { }

        // ハイライトの種類を示す識別子を返します。デフォルトでは0を返す。
        virtual int type() const { return 0; }
    };

    // Highlightクラスを継承し、将棋盤上の特定のマスをハイライトするためのクラス
    class FieldHighlight : public Highlight
    {
    public:
        // ハイライトの種類を示す定数。FieldHighlightでは1を使用する。
        enum { Type = 1 };

        // コンストラクタ。指定された筋（file）、段（rank）、色（color）でマスをハイライトする。
        FieldHighlight(int file, int rank, QColor color) : m_field(file, rank), m_color(color)
        {
        }

        // ハイライトされるマスの筋を返す。
        inline int file() const { return m_field.x(); }

        // ハイライトされるマスの段を返す。
        inline int rank() const { return m_field.y(); }

        // ハイライトされるマスの色を返す。
        inline QColor color() const { return m_color; }

        // ハイライトの種類を示す識別子を返す。
        int type() const { return Type; }

    private:
        // ハイライトするマスの位置（筋と段）
        QPoint m_field;

        // ハイライトされるマスの色
        QColor m_color;
    };

    // ShogiViewクラスのコンストラクタです。QWidgetを継承したクラスで、将棋盤のGUI部分を担当する。
    // このコンストラクタはexplicit指定し、不用意な暗黙の型変換を防ぐ。
    explicit ShogiView(QWidget *parent = nullptr);

    // m_boardにShogiBoardオブジェクトのポインタをセットする。
    // 将棋盤データが更新された際にウィジェットを再描画するための設定も行う。
    void setBoard(ShogiBoard* board);

    // 現在セットされている将棋盤オブジェクトへのポインタを返す。
    ShogiBoard* board() const;

    // 現在のマスのサイズを返す。
    QSize fieldSize() const;

    // 将棋盤ウィジェットの推奨サイズを計算して返す。
    QSize sizeHint() const override;

    // 盤面の状態（通常状態または反転状態）に応じて、指定された筋と段でのマスの矩形位置とサイズを計算する。
    // この関数は、マス自体の描画や、マス上に駒を配置する際に使用される基本的な位置情報を提供する。
    QRect calculateSquareRectangleBasedOnBoardState(const int file, const int rank) const;

    // 段番号や筋番号などのラベルを表示するための矩形を計算し、その表示位置を調整する。
    // この関数は、主に盤面の外側に配置される番号ラベルのために使用され、表示位置に適切なオフセットを加えることで、
    // 視覚的なレイアウトを整えるために利用する。
    QRect calculateRectangleForRankOrFileLabel(const int file, const int rank) const;

    // 駒文字と駒画像をm_piecesに格納する。
    void setPiece(char type, const QIcon &icon);

    // 指定された駒文字に対応するアイコンを返す。
    // typeは、駒を表す文字。例: 'P'（歩）、'L'（香車）など
    QIcon piece(QChar type) const;

    // ユーザーがクリックした位置を基に、マスを特定する。
    QPoint getClickedSquare(const QPoint& clickPosition) const;

    // ユーザーがクリックした位置を基に、通常モードでの将棋盤上のマスを特定する。
    QPoint getClickedSquareInDefaultState(const QPoint& clickPosition) const;

    // ユーザーがクリックした位置を基に、盤面が反転している状態での将棋盤上のマスを特定する。
    QPoint getClickedSquareInFlippedState(const QPoint& clickPosition) const;

    // 指定されたハイライトオブジェクトをハイライトリストに追加し、表示を更新する。
    void addHighlight(Highlight *hl);

    // 指定されたハイライトオブジェクトをハイライトリストから削除し、表示を更新する。
    void removeHighlight(Highlight *hl);

    // ハイライトリストから全てのデータを削除し、表示を更新する。
    void removeHighlightAllData();

    // 指定されたインデックスにあるハイライトオブジェクトへのポインタを返すインライン関数
    inline Highlight *highlight(int index) const { return m_highlights.at(index); }

    // ハイライトリストに格納されているハイライトオブジェクトの数を返すインライン関数
    inline int highlightCount() const { return m_highlights.size(); }

    // 先手側が画面手前の通常モード
    // 将棋の駒画像を各駒文字（1文字）にセットする。
    // 駒文字と駒画像をm_piecesに格納する。
    // m_piecesの型はQMap<char, QIcon>
    void setPieces();

    // 後手側が画面手前の反転モード
    // 将棋の駒画像を各駒文字（1文字）にセットする。
    // 駒文字と駒画像をm_piecesに格納する。
    // m_piecesの型はQMap<char, QIcon>
    void setPiecesFlip();

    // マウスクリックモードを設定する。
    void setMouseClickMode(bool mouseClickMode);

    // 盤面上の各マスのサイズをピクセル単位で返す。
    int squareSize() const;

    // 局面編集モードを設定する。
    void setPositionEditMode(bool positionEditMode);

    // 「全ての駒を駒台へ」をクリックした時に実行される。
    // 先手と後手の駒を同じ枚数にして全ての駒を駒台に載せる。
    void resetAndEqualizePiecesOnStands();

    // 将棋の平手初期局面に盤面を初期化する。
    void initializeToFlatStartingPosition();

    // 詰将棋の初期局面に盤面を初期化する。
    void shogiProblemInitialPosition();

    // 先手の配置を後手の配置に変更し、後手の配置を先手の配置に変更する。
    void flipBoardSides();

    // 将棋盤を反転させるフラグのgetter
    bool getFlipMode() const;

    // 将棋盤を反転させるフラグのsetter
    void setFlipMode(bool newFlipMode);

    // エラーが発生したかどうかを示すフラグを設定する。
    void setErrorOccurred(bool newErrorOccurred);

    // 対局時に指す駒を左クリックするとドラッグが開始される。
    void startDrag(const QPoint &from);

    // ドラッグを終了する。
    void endDrag();

    // 局面編集モードかどうかのフラグを返す。
    bool positionEditMode() const;

    // 先手の持ち時間をGUI上に表示するラベルを設定する。
    void setBlackClockText(const QString& text);

    // 後手の持ち時間をGUI上に表示するラベルを設定する。
    void setWhiteClockText(const QString& text);

    // 先手の持ち時間をGUI上に表示するラベルを返す。
    QLabel *blackClockLabel() const;

    // 後手の持ち時間をGUI上に表示するラベルを返す。
    QLabel *whiteClockLabel() const;

    // 先手の対局者名をGUI上に表示するラベルを設定する。
    void setBlackPlayerName(const QString& name);

    // 後手の対局者名をGUI上に表示するラベルを設定する。
    void setWhitePlayerName(const QString& name);

    // 先手の対局者名をGUI上に表示するラベルを返す。
    ElideLabel *blackNameLabel() const;

    // 後手の対局者名をGUI上に表示するラベルを返す。
    ElideLabel *whiteNameLabel() const;

    // すき間（マス単位）を動的に変更する。（0.0=密着, 1.0=1マス）
    void setStandGapCols(double cols);

    void setNameFontScale(double scale); // 0.2〜1.0 くらい

public slots:
    // マスのサイズを設定する。
    void setFieldSize(QSize fieldSize);

    // 盤面のサイズを拡大する。
    void enlargeBoard();

    // 盤面のサイズを縮小する。
    void reduceBoard();

signals:
    // マスのサイズが変更されたことを通知するシグナル
    void fieldSizeChanged(QSize fieldSize);

    // マスがクリックされたことを通知するシグナル
    void clicked(const QPoint&);

    // マスが右クリックされたことを通知するシグナル
    void rightClicked(const QPoint&);

    // エラーが発生したことを通知するシグナル
    void errorOccurred(const QString& errorMessage);

protected:
    // マウスボタンがクリックされた時のイベント処理を行う。
    void mouseMoveEvent(QMouseEvent* event) override;

    void resizeEvent(QResizeEvent* e) override;

private:
    // エラーが発生したかどうかを示すフラグ
    bool m_errorOccurred;

    // 将棋盤上の一つ一つのマス（square）のサイズ（size）をピクセル単位で表す変数
    int m_squareSize;

    int m_param1;

    int m_param2;

    // 盤を右にずらすオフセット
    int m_offsetX;

    // 盤を下にずらすオフセット
    int m_offsetY;

    // 将棋盤を反転させるフラグ
    // ノーマルモード:0、反転モード:1
    bool m_flipMode;

    // 将棋盤のポインタ
    QPointer<ShogiBoard> m_board;

    // マスのサイズ
    QSize m_fieldSize;

    // 駒文字と駒画像の対応を格納するマップ
    QMap<QChar, QIcon> m_pieces;

    // ハイライトオブジェクトのリスト
    QList<Highlight*> m_highlights;

    // マウスクリックモード
    bool m_mouseClickMode;

    // 局面編集モードかどうかを示すフラグ。
    bool m_positionEditMode;

    // ドラッグ中の状態を示すフラグ
    bool m_dragging;

    // 移動元のマス座標
    QPoint m_dragFrom;

    // ドラッグ中の駒文字
    QChar m_dragPiece;

    // 現在のマウス位置
    QPoint  m_dragPos;

    // 駒台からつまんだかどうかのフラグ
    bool m_dragFromStand;

    // ドラッグ中だけ使う一時的な枚数マップ
    QMap<QChar,int> m_tempPieceStandCounts;

    double m_nameFontScale { 0.36 }; // デフォルト倍率

    // 将棋盤、駒台を描画する。
    void paintEvent(QPaintEvent* event) override;

    // マウスボタンがクリックされた時のイベント処理を行う。
    void mouseReleaseEvent(QMouseEvent* event) override;

    // 盤面の指定された筋（file）に対応する筋番号を描画する。
    void drawFile(QPainter* painter, const int file) const;

    // 盤面の指定された段（rank）に対応する段番号を描画する。
    void drawRank(QPainter* painter, const int rank) const;

    // 盤面の指定されたマス（筋file、段rank）を描画する。
    void drawField(QPainter* painter, const int file, const int rank) const;

    // 先手駒台のマスを描画する。
    void drawBlackStandField(QPainter* painter, const int file, const int rank) const;

    // 後手駒台のマスを描画する。
    void drawWhiteStandField(QPainter* painter, const int file, const int rank) const;

    // 盤面の指定されたマス（筋file、段rank）に駒を描画する。
    void drawPiece(QPainter* painter, const int file, const int rank);

    // 先手駒台に置かれた駒を描画する。
    void drawBlackStandPiece(QPainter* painter, const int file, const int rank) const;

    // 後手駒台に置かれた駒を描画する。
    void drawWhiteStandPiece(QPainter* painter, const int file, const int rank) const;

    // 先手駒台に置かれた駒の枚数を描画する。
    void drawBlackStandPieceCount(QPainter* painter, const int file, const int rank) const;

    // 後手駒台に置かれた駒の枚数を描画する。
    void drawWhiteStandPieceCount(QPainter* painter, const int file, const int rank) const;

    // マスをハイライトする。
    void drawHighlights(QPainter* painter);

    // 将棋盤に星（目印となる4つの点）を描画する。
    void drawFourStars(QPainter* painter);

    // 局面編集モードで先手駒台に置かれた駒を描画する。
    void drawEditModeBlackStandPiece(QPainter* painter, const int file, const int rank) const;

    // 局面編集モードで先手駒台に置かれた駒の枚数を描画する。
    void drawEditModeBlackStandPieceCount(QPainter* painter, const int file, const int rank) const;

    // 局面編集モードで後手駒台に置かれた駒を描画する。
    void drawEditModeWhiteStandPiece(QPainter* painter, const int file, const int rank) const;

    // 局面編集モードで後手駒台に置かれた駒の枚数を描画する。
    void drawEditModeWhiteStandPieceCount(QPainter* painter, const int file, const int rank) const;

    // ドラッグ中の駒を描画する。
    void drawDraggingPiece();

    // 指定された段に対応する先手の駒の種類を返す。
    QChar rankToBlackShogiPiece(const int rank) const;

    // 指定された段に対応する後手の駒の種類を返す。
    QChar rankToWhiteShogiPiece(const int rank) const;

    // 将棋盤に段番号を描画する。
    void drawRanks(QPainter* painter);

    // 将棋盤に筋番号を描画する。
    void drawFiles(QPainter* painter);

    // 将棋盤の各マスを描画する。
    void drawBoardFields(QPainter* painter);

    // 局面編集モードで先手駒台のマスを描画する。
    void drawBlackEditModeStand(QPainter* painter);

    // 局面編集モードで後手駒台のマスを描画する。
    void drawWhiteEditModeStand(QPainter* painter);

    // 局面編集モードで先手と後手の駒台にある駒とその数を描画する。
    void drawEditModeStand(QPainter* painter);

    // 通常モードで先手駒台のマスを描画する。
    void drawBlackNormalModeStand(QPainter* painter);

    // 通常モードで後手駒台のマスを描画する。
    void drawWhiteNormalModeStand(QPainter* painter);

    // 通常モードで先手と後手の駒台を描画する。
    void drawNormalModeStand(QPainter* painter);

    // 局面編集モードと通常モードに応じた駒台の描画処理を行う。
    void drawEditModeFeatures(QPainter* painter);

    // 将棋盤の駒を描画する。
    void drawPieces(QPainter* painter);

    // 局面編集モードで先手駒台に置かれた駒とその枚数を描画する。
    void drawPiecesBlackStandInEditMode(QPainter* painter);

    // 局面編集モードで後手駒台に置かれた駒とその枚数を描画する。
    void drawPiecesWhiteStandInEditMode(QPainter* painter);

    // 駒台に置かれた駒とその数を描画する。
    void drawPiecesEditModeStandFeatures(QPainter* painter);

    // 通常モードで先手駒台に置かれた駒とその枚数を描画する。
    void drawPiecesBlackStandInNormalMode(QPainter* painter);

    // 通常モードで後手駒台に置かれた駒とその枚数を描画する。
    void drawPiecesWhiteStandInNormalMode(QPainter* painter);

    // 盤面サイズを更新する共通のロジックを抽出した関数
    void updateBoardSize();

    // 駒台の駒画像を描画する。
    void drawStandPieceIcon(QPainter* painter, const QRect& adjustedRect, QChar value) const;

    // 駒台の持ち駒の枚数を描画する。
    void drawStandPieceCount(QPainter* painter, const QRect& adjustedRect, QChar value) const;

    // 先手の持ち時間を表示するラベル
    QLabel* m_blackClockLabel{nullptr};

    // 後手の持ち時間を表示するラベル
    QLabel* m_whiteClockLabel{nullptr};

    // 先手の対局者名を表示するラベル
    ElideLabel* m_blackNameLabel{nullptr};

    // 後手の対局者名を表示するラベル
    ElideLabel* m_whiteNameLabel{nullptr};

    // 先手の対局者名と持ち時間を更新する。
    void updateBlackClockLabelGeometry();

    // 後手の対局者名と持ち時間を更新する。
    void updateWhiteClockLabelGeometry();

    // 先手駒台の外接矩形を計算する。
    QRect blackStandBoundingRect() const;

    // 後手駒台の外接矩形を計算する。
    QRect whiteStandBoundingRect() const;

    // 追加：文字列が矩形に収まる最大フォントサイズに調整
    void fitLabelFontToRect(QLabel* label, const QString& text,
                            const QRect& rect, int paddingPx = 2);

    // 盤・駒台レイアウトの再計算（m_squareSizeやギャップが変わったら呼ぶ）
    void recalcLayoutParams();

    // 駒台と盤の「すき間」を“マス何個ぶんか”で指定（既定 0.6 マス）
    double m_standGapCols{0.6};
    int    m_standGapPx   {0};    // m_squareSize から算出した実ピクセル

    // ラベル帯の厚み・余白・フォントサイズ（px/pt）
    int    m_labelGapPx   {8};     // 盤からの余白
    int    m_labelBandPx  {36};    // ラベル帯の厚み（縦 or 横）
    double m_labelFontPt  {12.0};  // 描画フォント（pt）

    // shogiview.h の private: に追加
    int boardLeftPx() const;
    int boardRightPx() const;
    int standInnerEdgePx(bool rightSide) const; // rightSide=true: 右側(先手)の内側端, false: 左側(後手)
};

#endif // SHOGIVIEW_H
