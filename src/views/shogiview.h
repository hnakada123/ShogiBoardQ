#ifndef SHOGIVIEW_H
#define SHOGIVIEW_H

/// @file shogiview.h
/// @brief 将棋盤面描画ビュークラスの定義


// ─────────────────────────────────────────────────────────────────────────────
// ShogiView
// 盤（9x9）と駒台を描画・操作する Qt ウィジェット。
// - 反転表示、局面編集モード、ドラッグ＆ドロップ、ハイライト表示などを提供
// - ShogiBoard（局面モデル）と接続して動作
// 設計メモ：
//  * QPainter の state 汚染を最小化するため、描画ヘルパは極力ローカルで save/restore する方針
//  * 駒台の疑似座標として file=10/11 を使用（左/右のスタンド列）
//  * 盤・駒台・座標の文字の位置は BoardGeometry（ウィジェット座標の値）を1度だけ計算して読む
//  * 対局者カード（名前・時計・手番の札）と編集用ボタンは BoardChromeWidget が持つ。
//    本クラスは盤・駒台の位置を渡すだけで、名前・時計・手番の操作は chrome() へ転送する
// ─────────────────────────────────────────────────────────────────────────────

#include "boardchromewidget.h"
#include "boardgeometry.h"
#include "boardstaticlayer.h"
#include "shogigamecontroller.h"
#include "shogiviewinteraction.h"
#include "boardcolors.h"
#include "boardvisuals.h"

#include <QIcon>
#include <QMap>
#include <QPixmap>
#include <QPointer>
#include <QRegion>
#include <QWidget>

#include <optional>

// 前方宣言（ヘッダ依存を軽減）
class ShogiBoard;
class ShogiViewHighlighting;
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

#ifdef SHOGIBOARDQ_TESTING
    /// テスト専用: tst_board_drag_repaint.cpp が盤面の変わらない部分の画像の使い回しを検証する
    friend class TestBoardDragRepaint;
#endif

    Q_PROPERTY(QSize fieldSize READ fieldSize NOTIFY fieldSizeChanged)

public:
    // ───────────────────────────── コンストラクタ ─────────────────────────────
    explicit ShogiView(QWidget *parent = nullptr);
    const BoardColors& boardColors() const { return m_boardColors; }
    const BoardVisuals& boardVisuals() const { return m_boardVisuals; }

    // ───────────────────────────── ボード接続 ────────────────────────────────
    void setBoard(ShogiBoard* board);   // モデル（局面）を差し替え
    ShogiBoard* board() const;          // 現在のモデル

    // ───────────────────────────── 盤サイズ系 ────────────────────────────────
    QSize fieldSize() const { return m_geometry.fieldSize(); }   // 1マスの大きさ（縦長）
    QSize sizeHint() const override;        // レイアウト用の推奨サイズ
    QSize minimumSizeHint() const override;  // 最小サイズ

    /// 盤・駒台・座標の文字の位置（ウィジェット座標）
    const BoardGeometry& boardGeometry() const { return m_geometry; }

    // 駒（成駒は生駒）を置く駒台セルのウィジェット座標
    QRect standPieceRect(Piece piece) const { return m_geometry.standCell(piece); }

    // 盤・駒台（疑似座標 file=10/11）・駒箱（file=12）のマスのウィジェット座標。該当しなければ空
    QRect fieldRect(const QPoint& field) const;

    // ───────────────────────────── 駒画像管理 ────────────────────────────────
    QIcon piece(Piece piece) const;               // 駒 → アイコン取得（盤の向きに合わせた画像）

    // ───────────────────────────── 入力座標変換 ──────────────────────────────
    /// ウィジェット座標の点がある盤・駒台（筋10/11）・駒箱（筋12）のマス。どこでもなければ QPoint()。
    /// 局面編集中に駒をドラッグしているときは、駒台のどこに落としてもその駒の置き場所を返す
    QPoint clickedSquare(const QPoint& clickPosition) const;

    // ───────────────────────────── 矢印表示（検討機能用） ─────────────────────
    // 盤上に矢印を描画するためのデータ構造
    struct Arrow {
        int fromFile = 0;   // 移動元の筋（1-9）、駒打ちの場合は0
        int fromRank = 0;   // 移動元の段（1-9）、駒打ちの場合は0
        int toFile = 0;     // 移動先の筋（1-9）
        int toRank = 0;     // 移動先の段（1-9）
        int priority = 0;   // 優先順位（1が最善手、2以上で数字を表示）
        Piece dropPiece = Piece::None;  // 駒打ちの場合の打つ側の駒、通常の移動は None
        QColor color = QColor(255, 0, 0, 200);  // 半透明の赤
        Qt::PenStyle penStyle = Qt::SolidLine;

        bool operator==(const Arrow& other) const
        {
            return fromFile == other.fromFile && fromRank == other.fromRank
                && toFile == other.toFile && toRank == other.toRank && priority == other.priority
                && dropPiece == other.dropPiece && color == other.color && penStyle == other.penStyle;
        }
        bool operator!=(const Arrow& other) const { return !(*this == other); }
    };
    void setArrows(const QList<Arrow>& arrows);  // 矢印をセット（複数可）
    const QList<Arrow>& arrows() const;            // 現在の候補手矢印
    void clearArrows();                            // 矢印をクリア

    // ───────────────────────────── 操作/状態切替 ────────────────────────────

    int  squareSize() const { return m_geometry.squareSize(); }   // マスの幅（px）
    void setSquareSize(int size);                // マスの幅を変えて盤の位置を計算し直す
    void setPositionEditMode(bool positionEditMode); // 局面編集モードON/OFF
    bool positionEditMode() const;

    QRect pieceBoxRect() const;                  // 編集中のみ有効な、左下の駒箱
    QRect pieceBoxCellRect(int rank) const;
    Turn pieceBoxSide() const { return m_interaction.pieceBoxSide(); }
    void setPieceBoxSide(Turn side);

    bool flipMode() const { return m_geometry.flipped(); }       // 反転表示か
    void setFlipMode(bool newFlipMode);          // 反転表示の切替（駒の画像も向きを合わせる）

    // ドラッグ操作
    void startDrag(const QPoint &from);
    void endDrag();

    // 対局者名の大きさ（マスの高さに対する比。0.2〜1.0）
    void setNameFontScale(double scale);

    // 対局者カード・時計・手番の札・編集用ボタン
    BoardChromeWidget* chrome() const { return m_chrome; }

    QImage toImage(qreal scale = 1.0);

    // board を適用して即再描画する（ピース読み込みも面倒見ます）
    void applyBoardAndRender(ShogiBoard* board);

    // レイアウト都合の固定サイズ指定は外側の責務にするのが基本ですが、
    // まとめたいならこちらも用意可（任意）
    void configureFixedSizing(int squarePx = -1);

    // ── 状態管理
    using Urgency = BoardChromeWidget::Urgency;

    // しきい値（ミリ秒）
    static constexpr qint64 kWarn10Ms = BoardChromeWidget::kWarn10Ms;
    static constexpr qint64 kWarn5Ms  = BoardChromeWidget::kWarn5Ms;

    // ハイライト/矢印の管理クラスへのアクセサ
    ShogiViewHighlighting* highlighting() const;

    /// 「編集終了」ボタンを出す・隠す（押すと chrome()->editExitRequested）
    void setEditExitButtonVisible(bool visible);

public slots:
    // 盤拡大/縮小（マスの幅を ±1）
    // emitSignal=falseの場合、fieldSizeChangedシグナルを発火しない
    // （イベントフィルターで処理する場合に使用）
    void enlargeBoard(bool emitSignal = true);
    void reduceBoard(bool emitSignal = true);

signals:
    void positionChanged();
    void fieldSizeChanged(QSize fieldSize);  // マスの大きさを変えた（拡大・縮小）
    void clicked(const QPoint&);             // 左クリック
    void rightClicked(const QPoint&);        // 右クリック

protected:
    // ───────────────────────────── QWidget オーバライド ─────────────────────
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* e) override;
    void wheelEvent(QWheelEvent* e) override;  // Ctrl+ホイールで拡大縮小

private:
    // ───────────────────────────── 描画ヘルパ（盤/駒/ラベル） ────────────────
    void drawRanksAndFiles(QPainter* painter) const;           // 段・筋の番号

    void drawPieces(QPainter* painter);                        // 盤の全駒
    void drawPiece(QPainter* painter, int file, int rank);

    // 駒台の持駒と枚数、局面編集中の駒箱
    void drawStandPieces(QPainter* painter);
    void drawStandPieceIcon(QPainter* painter, const QRect& cell, Piece piece) const;
    void drawPieceBoxBackground(QPainter* painter);
    void drawPieceBoxPieces(QPainter* painter);
    void relayoutPieceBoxSideSelector();

    // ───────────────────────────── 対局者カードの配置 ──────────────────────
    /// 盤・駒台の位置を BoardChromeWidget に渡して、カード・駒箱の切り替えを置き直す
    void relayoutChrome();

    /// 盤の位置を計算し直す（マスの大きさ・反転・盤を変えたとき）
    void  rebuildGeometry(int squareSize, bool flipped);
    void  applyBoardScaleChange(int squareSize, bool emitSignal);

private:
    // ───────────────────────────── 内部状態（モデル/描画/入力） ────────────────
    void loadPieceImages(bool flipped);
    void refreshPieceImages();
    void refreshBoardColors();
    void refreshBoardVisuals();
    BoardColors m_boardColors;
    BoardVisuals m_boardVisuals;

    // モデル
    QPointer<ShogiBoard> m_board;       // 局面。寿命は外部管理（QPointerで安全）

    // 盤・駒台・座標の文字の位置
    BoardGeometry m_geometry;

    // リソース（駒アイコン）
    QMap<Piece, QIcon>  m_pieces;       // 駒 → QIcon

    // paintEvent が描き直す範囲。ドラッグ中は駒の移動前後の周囲だけになるため、
    // 範囲に掛からない盤・駒・ラベルは描画命令ごと省く。
    QRegion m_paintRegion;
    bool intersectsPaintRegion(const QRectF& rect) const { return m_paintRegion.intersects(rect.toAlignedRect()); }
    // マスに描くもの（はみ出す駒の影を含む）を覆う範囲。部分的な再描画に使う
    QRect fieldPaintRect(const QPoint& field) const;
    // 盤のマス・駒台の枚数が変わったときに、その部分（局面編集中は駒箱も）だけを描き直す
    void onBoardSquareChanged(int file, int rank);
    void onBoardStandChanged(Piece piece);
    // 局面を丸ごと替えたとき（棋譜をたどる・待った・読み込みなど）は、画面に出ている駒と
    // 違うマス・駒台のセルだけを描き直す
    void onBoardReset();

    // 画面に出ている駒（盤はマスごと、駒台は駒ごとの枚数）。描いたときに記録し、
    // 描き直しの範囲に一部しか入らず記録と違う駒になったマスは不明（nullopt / 記録なし）にする。
    mutable QList<std::optional<Piece>> m_shownSquares;
    mutable QMap<Piece, int> m_shownStandCounts;
    bool paintRegionCovers(const QRect& rect) const;

    // 背景・木肌と縁・影・罫線・駒台・星（局面によらない部分）を描いた画像
    BoardStaticLayer m_staticLayer;

    // 入力/ドラッグ（ShogiViewInteraction に委譲）
    ShogiViewInteraction m_interaction;

    // ハイライト/矢印の管理クラス
    ShogiViewHighlighting* m_highlighting = nullptr;

    // 対局者カード・時計・手番の札・編集用ボタン（子ウィジェットとして盤に重ねる）
    BoardChromeWidget* m_chrome = nullptr;
};

#endif // SHOGIVIEW_H
