#ifndef SHOGIVIEW_H
#define SHOGIVIEW_H

// ─────────────────────────────────────────────────────────────────────────────
// ShogiView
// 盤（9x9）と駒台を描画・操作する Qt ウィジェット。
// - 反転表示、局面編集モード、ドラッグ＆ドロップ、ハイライト表示、時計/名前ラベル表示などを提供
// - ShogiBoard（局面モデル）と接続して動作
// 設計メモ：
//  * QPainter の state 汚染を最小化するため、描画ヘルパは極力ローカルで save/restore する方針
//  * 駒台の疑似座標として file=10/11 を使用（左/右のスタンド列）
//  * ラベル（時計/名前）はウィジェット子として配置し、レイアウトは本クラスで制御
// ─────────────────────────────────────────────────────────────────────────────

#include "elidelabel.h"
#include <QIcon>
#include <QMap>
#include <QPointer>
#include <QWidget>

// 前方宣言（ヘッダ依存を軽減）
class ShogiBoard;
class GlobalToolTip;
class QLabel;
class QPainter;
class QEvent;
class QMouseEvent;
class QResizeEvent;
class QColor;
class QPoint;
class QRect;
class QSize;
class QString;

class ShogiView : public QWidget
{
    Q_OBJECT
    // Q_PROPERTY: QML/Qt Designer 等からマスサイズ（正方）をバインド可能にする
    Q_PROPERTY(QSize fieldSize READ fieldSize WRITE setFieldSize NOTIFY fieldSizeChanged)

public:
    // ──────────────────────────── ハイライト種別 ─────────────────────────────
    // 視覚強調を表す基底クラス。必要に応じて派生種別を追加可能。
    class Highlight
    {
    public:
        Highlight() {}
        virtual ~Highlight() {}
        virtual int type() const { return 0; }  // 型識別子
    };

    // 1 マス単位の矩形ハイライト（色つき）
    class FieldHighlight : public Highlight
    {
    public:
        enum { Type = 1 };
        FieldHighlight(int file, int rank, QColor color) : m_field(file, rank), m_color(color) {}
        inline int file()        const { return m_field.x(); }
        inline int rank()        const { return m_field.y(); }
        inline QColor color()    const { return m_color; }
        int type() const override { return Type; }
    private:
        QPoint m_field;  // (file, rank)
        QColor m_color;
    };

    // ───────────────────────────── コンストラクタ ─────────────────────────────
    explicit ShogiView(QWidget *parent = nullptr);

    // ───────────────────────────── ボード接続 ────────────────────────────────
    void setBoard(ShogiBoard* board);   // モデル（局面）を差し替え
    ShogiBoard* board() const;          // 現在のモデル

    // ───────────────────────────── 盤サイズ系 ────────────────────────────────
    QSize fieldSize() const;            // 1マスの QSize（正方形推奨）
    QSize sizeHint() const override;    // レイアウト用の推奨サイズ
    void updateBoardSize();             // m_squareSize を反映して再配置（ユーティリティ）

    // 盤/ラベル座標計算（反転考慮）
    QRect calculateSquareRectangleBasedOnBoardState(int file, int rank) const;
    QRect calculateRectangleForRankOrFileLabel(int file, int rank) const;

    // ───────────────────────────── 駒画像管理 ────────────────────────────────
    void  setPiece(char type, const QIcon &icon); // 駒文字 → アイコン登録
    QIcon piece(QChar type) const;                // 駒文字 → アイコン取得
    void  setPieces();                            // 通常向きの画像一括登録
    void  setPiecesFlip();                        // 反転向きの画像一括登録

    // ───────────────────────────── 入力座標変換 ──────────────────────────────
    QPoint getClickedSquare(const QPoint& clickPosition) const;            // エントリ
    QPoint getClickedSquareInDefaultState(const QPoint& clickPosition) const; // 非反転
    QPoint getClickedSquareInFlippedState(const QPoint& clickPosition) const; // 反転

    // ───────────────────────────── ハイライト管理 ───────────────────────────
    void addHighlight(Highlight *hl);            // 追加（所有権ポリシーは呼び出し側設計に依存）
    void removeHighlight(Highlight *hl);         // 1件削除
    void removeHighlightAllData();               // 全削除
    inline Highlight *highlight(int index) const { return m_highlights.at(index); }
    inline int highlightCount() const { return m_highlights.size(); }

    // ───────────────────────────── 操作/状態切替 ────────────────────────────
    void setMouseClickMode(bool mouseClickMode); // クリック操作フラグ
    int  squareSize() const;                     // m_squareSize（px）
    void setPositionEditMode(bool positionEditMode); // 局面編集モードON/OFF
    bool positionEditMode() const;

    void resetAndEqualizePiecesOnStands();       // 初期化（平手/駒台均し等は ShogiBoard 実装に依存）
    void initializeToFlatStartingPosition();     // 平手初期局面を適用
    void shogiProblemInitialPosition();          // 問題用初期局面を適用
    void flipBoardSides();                       // 先後入替（モデル側で処理）

    bool getFlipMode() const;                    // 反転表示か
    void setFlipMode(bool newFlipMode);          // 反転表示の切替

    void setErrorOccurred(bool newErrorOccurred);// エラーフラグ

    // ドラッグ操作
    void startDrag(const QPoint &from);
    void endDrag();

    // 時計テキスト
    void setBlackClockText(const QString& text);
    void setWhiteClockText(const QString& text);
    QLabel *blackClockLabel() const;
    QLabel *whiteClockLabel() const;

    // プレイヤー名
    void setBlackPlayerName(const QString& name);
    void setWhitePlayerName(const QString& name);
    ElideLabel *blackNameLabel() const;
    ElideLabel *whiteNameLabel() const;

    // レイアウト調整パラメータ
    void setStandGapCols(double cols);      // 盤‐駒台の横ギャップ（列数 0.0〜2.0）
    void setNameFontScale(double scale);    // 0.2〜1.0 程度
    void setRankFontScale(double scale);    // 0.5〜1.2 程度

    // 手番表示（true=先手手番, false=後手手番）
    void setActiveSide(bool blackTurn);

    // 手番ハイライト配色（任意）
    void setHighlightStyle(const QColor& bgOn, const QColor& fgOn, const QColor& fgOff);

public slots:
    // Q_PROPERTY 用 setter（レイアウト更新・ラベル再配置を含む）
    void setFieldSize(QSize fieldSize);
    // 盤拡大/縮小（m_squareSize を ±1）
    void enlargeBoard();
    void reduceBoard();

signals:
    void fieldSizeChanged(QSize fieldSize);  // setFieldSize 内で発火
    void clicked(const QPoint&);             // 左クリック
    void rightClicked(const QPoint&);        // 右クリック
    void errorOccurred(const QString& errorMessage);

protected:
    // ───────────────────────────── QWidget オーバライド ─────────────────────
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* e) override;
    bool eventFilter(QObject* obj, QEvent* ev) override;

private:
    // ───────────────────────────── 描画ヘルパ（盤/駒/ラベル） ────────────────
    void drawFiles(QPainter* painter);                         // 全筋ラベル
    void drawFile(QPainter* painter, int file) const;          // 筋ラベル 1 本
    void drawRanks(QPainter* painter);                         // 全段ラベル
    void drawRank(QPainter* painter, int rank) const;          // 段ラベル 1 本

    void drawBoardFields(QPainter* painter);                   // 盤の全マス
    void drawField(QPainter* painter, int file, int rank) const;

    void drawPieces(QPainter* painter);                        // 盤の全駒
    void drawPiece(QPainter* painter, int file, int rank);

    void drawFourStars(QPainter* painter);                     // 4隅の星（装飾）
    void drawHighlights(QPainter* painter);                    // ハイライト（含む駒台疑似座標）

    // 駒台（マス背景）
    void drawBlackStandField(QPainter* painter, int file, int rank) const;
    void drawWhiteStandField(QPainter* painter, int file, int rank) const;

    // 駒台（駒・枚数：通常モード）
    void drawBlackStandPiece(QPainter* painter, int file, int rank) const;
    void drawWhiteStandPiece(QPainter* painter, int file, int rank) const;
    void drawBlackStandPieceCount(QPainter* painter, int file, int rank) const;
    void drawWhiteStandPieceCount(QPainter* painter, int file, int rank) const;

    // 駒台（駒・枚数：編集モード）
    void drawEditModeBlackStandPiece(QPainter* painter, int file, int rank) const;
    void drawEditModeBlackStandPieceCount(QPainter* painter, int file, int rank) const;
    void drawEditModeWhiteStandPiece(QPainter* painter, int file, int rank) const;
    void drawEditModeWhiteStandPieceCount(QPainter* painter, int file, int rank) const;
    void drawEditModeFeatures(QPainter* painter);

    // 表示モード別の駒台領域/駒描画
    void drawBlackEditModeStand(QPainter* painter);
    void drawWhiteEditModeStand(QPainter* painter);
    void drawEditModeStand(QPainter* painter);
    void drawBlackNormalModeStand(QPainter* painter);
    void drawWhiteNormalModeStand(QPainter* painter);
    void drawNormalModeStand(QPainter* painter);
    void drawPiecesBlackStandInEditMode(QPainter* painter);
    void drawPiecesWhiteStandInEditMode(QPainter* painter);
    void drawPiecesBlackStandInNormalMode(QPainter* painter);
    void drawPiecesWhiteStandInNormalMode(QPainter* painter);
    void drawPiecesEditModeStandFeatures(QPainter* painter);

    // ドラッグ中の駒（カーソル追従）
    void drawDraggingPiece(QPainter* painter);

    // 駒台の駒・枚数描画の共通ロジック
    void drawStandPieceIcon(QPainter* painter, const QRect& adjustedRect, QChar value) const;
    void drawStandPieceCount(QPainter* painter, const QRect& adjustedRect, QChar value) const;

    // 駒台の段→駒文字マッピング
    QChar rankToBlackShogiPiece(int rank) const;
    QChar rankToWhiteShogiPiece(int rank) const;

    // ───────────────────────────── ラベルとレイアウト ──────────────────────
    void updateBlackClockLabelGeometry();   // 黒側ラベルのジオメトリ更新
    void updateWhiteClockLabelGeometry();   // 白側ラベルのジオメトリ更新
    QRect blackStandBoundingRect() const;   // 黒側駒台の外接矩形
    QRect whiteStandBoundingRect() const;   // 白側駒台の外接矩形
    void  recalcLayoutParams();             // 内部レイアウト定数の再計算

    // フォントを矩形にフィットさせる（時計用）
    void fitLabelFontToRect(QLabel* label, const QString& text,
                            const QRect& rect, int paddingPx = 2);

    // 盤・駒台境界のユーティリティ
    int  boardLeftPx()  const;
    int  boardRightPx() const;
    int  standInnerEdgePx(bool rightSide) const;
    int  minGapForRankLabelsPx() const;

    // 名前ラベル/ツールチップ
    void    refreshNameLabels();                 // 向きマーク付与の表示更新
    static QString stripMarks(const QString&);   // ▲▼▽△ の除去

    // 手番ハイライト
    void   applyTurnHighlight(bool blackActive);

private:
    // ───────────────────────────── 内部状態（モデル/描画/入力） ────────────────
    // モデル
    QPointer<ShogiBoard> m_board;       // 局面。寿命は外部管理（QPointerで安全）

    // 描画寸法・配置
    int   m_squareSize { 50 };          // 1マスの基準ピクセル（正方）
    QSize m_fieldSize;                  // 1マスの QSize（正方前提だが将来拡張可）

    int   m_param1 { 0 };               // 先手側スタンドの水平寄せ量（px）
    int   m_param2 { 0 };               // 後手側スタンドの水平寄せ量（px）
    int   m_offsetX{ 0 };               // 盤の左端 X（boardLeftPx 相当）
    int   m_offsetY{ 20 };              // 盤の上端 Y のオフセット
    bool  m_flipMode{ false };          // 反転表示

    // ラベル帯とフォント
    int    m_labelGapPx  { 8 };         // 盤とラベルのすき間
    int    m_labelBandPx { 36 };        // ラベル帯の厚み
    double m_labelFontPt { 12.0 };      // ラベル用の基準フォントサイズ（pt）
    double m_nameFontScale { 0.36 };    // 名前ラベルのスケール
    double m_rankFontScale { 0.8 };     // 段ラベルのスケール

    // 盤‐駒台ギャップ
    double m_standGapCols { 0.6 };      // 列数換算（0.0〜2.0）
    int    m_standGapPx   { 0 };        // 実効ギャップ（px）

    // リソース（駒アイコン・ハイライト）
    QMap<QChar, QIcon>  m_pieces;       // 駒文字 → QIcon
    QList<Highlight*>   m_highlights;   // ハイライト（所有権は呼び出し側設計に依存）

    // ラベル（子ウィジェット）
    QLabel*     m_blackClockLabel { nullptr };
    QLabel*     m_whiteClockLabel { nullptr };
    ElideLabel* m_blackNameLabel  { nullptr };
    ElideLabel* m_whiteNameLabel  { nullptr };

    // プレイヤー名（装飾抜きの素の文字列）
    QString m_blackNameBase;
    QString m_whiteNameBase;

    // 入力/ドラッグ
    bool   m_mouseClickMode   { true };   // クリック操作モード
    bool   m_positionEditMode { false };  // 局面編集モード
    bool   m_dragging         { false };  // ドラッグ中
    QPoint m_dragFrom;                    // つまみ上げ元のマス
    QChar  m_dragPiece { ' ' };           // ドラッグ中の駒文字
    QPoint m_dragPos;                     // カーソル座標（ウィジェット系）
    bool   m_dragFromStand { false };     // 駒台からのドラッグか
    QMap<QChar,int> m_tempPieceStandCounts; // ドラッグ中の一時在庫

    // 手番ハイライト
    QColor m_highlightBg    = QColor(255, 255, 0);
    QColor m_highlightFgOn  = QColor(0, 0, 255);
    QColor m_highlightFgOff = QColor(51, 51, 51);
    bool   m_blackActive    = true;

    // エラー状態
    bool   m_errorOccurred { false };

    // 自前ツールチップ
    GlobalToolTip* m_tooltip = nullptr;
};

#endif // SHOGIVIEW_H
