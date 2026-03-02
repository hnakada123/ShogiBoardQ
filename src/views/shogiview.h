#ifndef SHOGIVIEW_H
#define SHOGIVIEW_H

/// @file shogiview.h
/// @brief 将棋盤面描画ビュークラスの定義


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
#include "shogigamecontroller.h"
#include "shogiviewinteraction.h"
#include "shogiviewlayout.h"

#include <QIcon>
#include <QHash>
#include <QMap>
#include <QPointer>
#include <QPixmap>
#include <QWidget>
#include <QPushButton>

// 前方宣言（ヘッダ依存を軽減）
class ShogiBoard;
class ShogiViewHighlighting;
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
        virtual ~Highlight();
        virtual int type() const { return 0; }  // 型識別子
    };

    // 1 マス単位の矩形ハイライト（色つき）
    class FieldHighlight : public Highlight
    {
    public:
        enum { Type = 1 };
        FieldHighlight(int file, int rank, QColor color) : m_field(file, rank), m_color(color) {}
        ~FieldHighlight() override;
        int file()        const { return m_field.x(); }
        int rank()        const { return m_field.y(); }
        QColor color()    const { return m_color; }
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
    QSize sizeHint() const override;        // レイアウト用の推奨サイズ
    QSize minimumSizeHint() const override;  // 最小サイズ
    void updateBoardSize();             // m_squareSize を反映して再配置（ユーティリティ）

    // 盤/ラベル座標計算（反転考慮）
    QRect calculateSquareRectangleBasedOnBoardState(int file, int rank) const;
    QRect calculateRectangleForRankOrFileLabel(int file, int rank) const;

    // キャッシュ済みマス矩形（描画用：レイアウト変更時に自動再構築）
    QRect cachedFieldRect(int file, int rank) const;
    void  invalidateFieldRectCache();

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
    Highlight *highlight(int index) const;
    int highlightCount() const;

    // ───────────────────────────── 矢印表示（検討機能用） ─────────────────────
    // 盤上に矢印を描画するためのデータ構造
    struct Arrow {
        int fromFile = 0;   // 移動元の筋（1-9）、駒打ちの場合は0
        int fromRank = 0;   // 移動元の段（1-9）、駒打ちの場合は0
        int toFile = 0;     // 移動先の筋（1-9）
        int toRank = 0;     // 移動先の段（1-9）
        int priority = 0;   // 優先順位（1が最善手、2以上で数字を表示）
        QChar dropPiece = ' ';  // 駒打ちの場合の駒種（例: 'P', 'G' など）、通常の移動は空白
        QColor color = QColor(255, 0, 0, 200);  // 半透明の赤
    };
    void setArrows(const QList<Arrow>& arrows);  // 矢印をセット（複数可）
    void clearArrows();                            // 矢印をクリア

    // ───────────────────────────── 操作/状態切替 ────────────────────────────
    void setMouseClickMode(bool mouseClickMode); // クリック操作フラグ
    int  squareSize() const;                     // m_squareSize（px）
    void setSquareSize(int size);                // m_squareSize を設定してレイアウト再計算
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
    
    // 時計表示の有効/無効
    void setClockEnabled(bool enabled);
    bool isClockEnabled() const;

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

    // 追加: 残時間(ミリ秒)を受け取るセッター
    void setBlackTimeMs(qint64 ms);
    void setWhiteTimeMs(qint64 ms);

    QImage toImage(qreal scale = 1.0);

    // board を適用して即再描画する（ピース読み込みも面倒見ます）
    void applyBoardAndRender(ShogiBoard* board);

    // レイアウト都合の固定サイズ指定は外側の責務にするのが基本ですが、
    // まとめたいならこちらも用意可（任意）
    void configureFixedSizing(int squarePx = -1);

    void clearTurnHighlight();

    void setUiMuted(bool on);
    bool uiMuted() const { return m_uiMuted; }

    void setActiveIsBlack(bool activeIsBlack);

    // 対局終了時のスタイル維持（trueの間はclearTurnHighlightを無視）
    void setGameOverStyleLock(bool locked);
    bool gameOverStyleLock() const { return m_gameOverStyleLock; }

    // ── 状態管理
    enum class Urgency { Normal, Warn10, Warn5 };

    // しきい値（ミリ秒）
    static constexpr qint64 kWarn10Ms = 10'000;
    static constexpr qint64 kWarn5Ms  = 5'000;

    // ハイライト/矢印/手番表示の管理クラスへのアクセサ
    ShogiViewHighlighting* highlighting() const;

    void setUrgencyVisuals(Urgency u);

    void updateTurnIndicator(ShogiGameController::Player now);

    void relayoutEditExitButton();

public slots:
    // Q_PROPERTY 用 setter（レイアウト更新・ラベル再配置を含む）
    void setFieldSize(QSize fieldSize);
    // 盤拡大/縮小（m_squareSize を ±1）
    // emitSignal=falseの場合、fieldSizeChangedシグナルを発火しない
    // （イベントフィルターで処理する場合に使用）
    void enlargeBoard(bool emitSignal = true);
    void reduceBoard(bool emitSignal = true);

    // 手番側の残り時間に応じて“名前/時計”の配色を切り替える
    void applyClockUrgency(qint64 activeRemainMs);

signals:
    void fieldSizeChanged(QSize fieldSize);  // setFieldSize 内で発火
    void clicked(const QPoint&);             // 左クリック
    void rightClicked(const QPoint&);        // 右クリック
    void highlightsCleared();                // removeHighlightAllData() で発火

protected:
    // ───────────────────────────── QWidget オーバライド ─────────────────────
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* e) override;
    void wheelEvent(QWheelEvent* e) override;  // Ctrl+ホイールで拡大縮小
    bool eventFilter(QObject* obj, QEvent* ev) override;

private:
    // ───────────────────────────── サイズ自動調整 ────────────────────────────
    void fitBoardToWidget();  // ウィジェットサイズに合わせて将棋盤を自動調整

    // ───────────────────────────── 描画ヘルパ（盤/駒/ラベル） ────────────────
    void drawFiles(QPainter* painter);                         // 全筋ラベル
    void drawFile(QPainter* painter, int file) const;          // 筋ラベル 1 本
    void drawRanks(QPainter* painter);                         // 全段ラベル
    void drawRank(QPainter* painter, int rank) const;          // 段ラベル 1 本

    void drawBackground(QPainter* painter);                    // E1: 背景グラデーション
    void drawBoardShadow(QPainter* painter);                   // 将棋盤の影（立体感）
    void drawBoardMargin(QPainter* painter);                   // 将棋盤の余白部分を描画
    void drawStandShadow(QPainter* painter);                   // 駒台の影（立体感）
    void drawBoardFields(QPainter* painter);                   // 盤の全マス
    void drawField(QPainter* painter, int file, int rank) const;

    void drawPieces(QPainter* painter);                        // 盤の全駒
    void drawPiece(QPainter* painter, int file, int rank);

    void drawFourStars(QPainter* painter);                     // 4隅の星（装飾）

    // 駒台（マス背景）
    void drawBlackStandField(QPainter* painter, int file, int rank) const;
    void drawWhiteStandField(QPainter* painter, int file, int rank) const;

    // 駒台（駒・枚数：通常モード）
    void drawBlackStandPiece(QPainter* painter, int file, int rank) const;
    void drawWhiteStandPiece(QPainter* painter, int file, int rank) const;

    // 表示モード別の駒台領域/駒描画
    void drawBlackNormalModeStand(QPainter* painter);
    void drawWhiteNormalModeStand(QPainter* painter);
    void drawNormalModeStand(QPainter* painter);
    void drawPiecesBlackStandInNormalMode(QPainter* painter);
    void drawPiecesWhiteStandInNormalMode(QPainter* painter);
    void drawPiecesStandFeatures(QPainter* painter);

    // ドラッグ中の駒描画は m_interaction に委譲

    // 駒台の駒・枚数描画の共通ロジック
    void drawStandPieceIcon(QPainter* painter, const QRect& adjustedRect, QChar value) const;

    // 駒台の段→駒文字マッピング
    QChar rankToBlackShogiPiece(const int file, const int rank) const;
    QChar rankToWhiteShogiPiece(const int file, const int rank) const;

    // ───────────────────────────── ラベルとレイアウト ──────────────────────
    void updateBlackClockLabelGeometry();   // 黒側ラベルのジオメトリ更新
    void updateWhiteClockLabelGeometry();   // 白側ラベルのジオメトリ更新

    // フォントを矩形にフィットさせる（時計用）
    void fitLabelFontToRect(QLabel* label, const QString& text,
                            const QRect& rect, int paddingPx = 2);

    // レイアウト委譲ヘルパ（m_board 経由で盤サイズを渡す）
    QRect blackStandBoundingRect() const;
    QRect whiteStandBoundingRect() const;
    int   boardLeftPx() const;
    int   boardRightPx() const;
    int   standInnerEdgePx(bool rightSide) const;
    void  recalcLayoutParams();

    // 名前ラベル/ツールチップ
    void    refreshNameLabels();                 // 向きマーク付与の表示更新
    static QString stripMarks(const QString&);   // ▲▼▽△ の除去

private:
    // ───────────────────────────── 内部状態（モデル/描画/入力） ────────────────
    // モデル
    QPointer<ShogiBoard> m_board;       // 局面。寿命は外部管理（QPointerで安全）

    // レイアウト計算を委譲するクラス
    ShogiViewLayout m_layout;

    // リソース（駒アイコン）
    QMap<QChar, QIcon>  m_pieces;       // 駒文字 → QIcon
    mutable QHash<quint64, QPixmap> m_standPiecePixmapCache; // 駒台描画用pixmapキャッシュ

    // マス矩形キャッシュ（レイアウト変更時に無効化、描画時に遅延再構築）
    mutable QHash<quint64, QRect> m_fieldRectCache;
    mutable bool m_fieldRectCacheValid = false;

    // ラベル（子ウィジェット）
    QLabel*     m_blackClockLabel { nullptr };
    QLabel*     m_whiteClockLabel { nullptr };
    ElideLabel* m_blackNameLabel  { nullptr };
    ElideLabel* m_whiteNameLabel  { nullptr };
    bool        m_clockEnabled    { true };    // 時計表示の有効/無効

    // プレイヤー名（装飾抜きの素の文字列）
    QString m_blackNameBase;
    QString m_whiteNameBase;

    // 入力/ドラッグ（ShogiViewInteraction に委譲）
    ShogiViewInteraction m_interaction;


    // エラー状態
    bool   m_errorOccurred { false };

    // 自前ツールチップ
    GlobalToolTip* m_tooltip = nullptr;

    // ハイライト/矢印/手番表示の管理クラス
    ShogiViewHighlighting* m_highlighting = nullptr;

    // 追加: ログ用に保持
    qint64 m_blackTimeMs = -1;
    qint64 m_whiteTimeMs = -1;

    // ログスパム防止（前回値と同じなら出力しない）
    qint64 m_lastLoggedBlackMs = -2;
    qint64 m_lastLoggedWhiteMs = -2;

    bool m_uiMuted = false;
    bool m_gameOverStyleLock = false;  // 対局終了時のスタイル維持

    void ensureTurnLabels();
    void relayoutTurnLabels();

    void ensureAndPlaceEditExitButton();

    void styleEditExitButton(QPushButton* btn);
    void fitEditExitButtonFont(QPushButton* btn, int maxWidth);
};

#endif // SHOGIVIEW_H
