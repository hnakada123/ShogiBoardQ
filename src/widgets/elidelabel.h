#ifndef ELIDELABEL_H
#define ELIDELABEL_H

/// @file elidelabel.h
/// @brief テキスト省略表示ラベルクラスの定義


#include <QLabel>
#include <QTimer>

/**
 * @brief ラベル内テキストが幅に収まらない場合に視認性を高める QLabel 派生クラス
 *
 * - 通常はエリプシス（…）で省略表示
 * - ホバー中は自動で横スクロール（任意）
 * - 左ドラッグで手動パン（任意）
 * - 下線の簡易装飾（任意）
 *
 * スクロールは QTimer による一定ピクセル移動（m_pxPerTick / m_intervalMs）。
 *
 */
class ElideLabel : public QLabel
{
    Q_OBJECT
public:
    // コンストラクタ
    // 役割：サイズポリシーと自動スクロール用タイマーを初期化（タイマーは未起動）。
    explicit ElideLabel(QWidget* parent = nullptr);

    // フルテキストを設定するセッター。
    // 役割：表示の元になる完全な文字列を更新し、ツールチップ／省略文字列を再計算。
    // 注意：同一文字列なら何もしない（不要な再計算・再描画を抑制）。
    void setFullText(const QString& text);

    // 現在のフルテキストを返す。
    // 役割：省略やスクロールの元データ参照用。
    QString fullText() const;

    // エリプシス位置（左/中/右）を設定。
    // 役割：Qt::TextElideMode を更新し、幅に合わせて省略文字列を再計算。
    void setElideMode(Qt::TextElideMode mode);

    // 現在のエリプシス位置を取得。
    Qt::TextElideMode elideMode() const;

    // 追加：ホバー自動スクロール / ドラッグ手動パン / 下線
    // 幅不足のときだけホバーで自動スクロールするかを切り替え。
    void setSlideOnHover(bool on);
    // 自動スクロールの速度（1ティックあたりの移動ピクセル）。役割：最低 1px に丸め。
    void setSlideSpeed(int pxPerTick);
    // 自動スクロールのティック間隔（ms）。役割：稼働中なら即座に反映。
    void setSlideInterval(int ms);
    // 下線を描くか（簡易装飾）。役割：paintEvent で 1px ラインを引く。
    void setUnderline(bool on);
    // 左ドラッグでの手動パンを許可。役割：押下/移動/解放イベントで参照。
    void setManualPanEnabled(bool on);

    // サイズヒント
    // 役割：高さはフォントに依存、幅はレイアウトに任せる（QLabel 既定を踏襲）。
    QSize sizeHint() const override;

protected:
    // 描画イベント
    // 役割：通常はエリプシス描画、スクロール中（自動 or ドラッグ）かつ溢れ時はループ描画。
    void paintEvent(QPaintEvent*) override;

    // リサイズ時は省略文字列を再計算して表示更新。
    void resizeEvent(QResizeEvent*) override;

#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
    // ホバー開始（Qt 6+）：自動スクロール開始条件を再評価。
    void enterEvent(QEnterEvent*) override;
#else
    // ホバー開始（Qt 5）：自動スクロール開始条件を再評価。
    void enterEvent(QEvent*) override;
#endif

    // ホバー終了：自動スクロール停止＆オフセットを先頭に戻す。
    void leaveEvent(QEvent*) override;

    // 手動パン：左押下でドラッグ開始（条件を満たすとき）。
    void mousePressEvent(QMouseEvent*) override;
    // 手動パン：ドラッグ中は差分でオフセット更新。
    void mouseMoveEvent(QMouseEvent*) override;
    // 手動パン：解放で終了し、必要なら自動スクロールを再開。
    void mouseReleaseEvent(QMouseEvent*) override;

private slots:
    // 自動スクロール用タイマーのタイムアウトハンドラ
    void onTimerTimeout();

private:
    // 省略文字列を最新化。
    // 役割：contentsRect().width() に基づいて elidedText を作り直し、必要ならスクロール開始判定。
    void updateElidedText();

    // 自動スクロール開始条件を評価して起動。
    // 役割：ホバー中・はみ出し・非ドラッグ・許可フラグの全条件を満たしたときのみ開始。
    void startSlideIfNeeded();

    // 自動スクロール停止（タイマー停止＆カーソル復帰）。
    void stopSlide();

    // はみ出し判定（フルテキスト幅 > 内容幅）。
    bool isOverflowing() const;

    // ───────── 内部状態 ─────────
    QString m_fullText;                    // 元テキスト
    QString m_elidedText;                  // 省略済みテキスト（表示用キャッシュ）
    Qt::TextElideMode m_mode = Qt::ElideMiddle; // 省略位置

    // スライド関連
    bool   m_slideOnHover = false;         // ホバー時の自動スクロール許可
    bool   m_manualPan    = false;         // ドラッグ手動パン許可
    bool   m_underline    = false;         // 下線装飾フラグ
    QTimer m_timer;                         // 自動スクロール用タイマー
    int    m_offset       = 0;             // 描画オフセット(px)
    int    m_gap          = 24;            // ループ時の空白（継ぎ目を隠す）
    int    m_pxPerTick    = 2;             // 自動スクロール速度(px/ティック)
    int    m_intervalMs   = 16;            // タイマー間隔(ms) ≒ 60fps
    int    m_lastDragX    = 0;             // 手動パンのドラッグ基準X
    bool   m_dragging     = false;         // ドラッグ中か
};

#endif // ELIDELABEL_H
