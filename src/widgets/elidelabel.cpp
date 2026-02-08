/// @file elidelabel.cpp
/// @brief テキスト省略表示ラベルクラスの実装

#include "elidelabel.h"
#include <QPainter>
#include <QMouseEvent>

/*
 * ElideLabel
 * ラベル内テキストが「幅に収まらない場合だけ」視認性を高める工夫を行う QLabel 派生。
 * 役割：
 *  - 通常はエリプシス（…）で省略表示
 *  - ホバー中は自動で横スクロール（任意）
 *  - 左ドラッグで手動パン（任意）
 *  - 下線の簡易装飾（任意）
 * 備考：スクロールは QTimer による一定ピクセル移動（m_pxPerTick / m_intervalMs）。
 */
ElideLabel::ElideLabel(QWidget* parent)
    : QLabel(parent)
{
    // 横方向はレイアウトに合わせて広がり、縦方向はフォントに依存する固定高さにする。
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    // 自動スクロール用タイマーのハンドラを設定。
    // 役割：一定間隔ごとに表示オフセット m_offset を増分し、update() で再描画を促す。
    // 注意：実際の開始/停止は startSlideIfNeeded()/stopSlide() が受け持つ（常時スクロールしない）。
    connect(&m_timer, &QTimer::timeout, this, &ElideLabel::onTimerTimeout);
}

// フルテキストを設定するセッター。
// 役割：表示の元になる完全な文字列を更新し、ツールチップ／省略文字列を再計算。
// 注意：同一文字列なら何もしない（不要な再計算・再描画を抑制）。
// 副作用：ツールチップも新しいフルテキストに置き換える。
// 補足：スクロール位置（m_offset）は維持する仕様。
//       変更時にスクロールを先頭へ戻したい場合は、m_offset=0; の追加や start/stop を検討。
void ElideLabel::setFullText(const QString& t) {
    if (m_fullText == t) return;   // 変更なしなら早期 return（最適化）
    m_fullText = t;                 // 元テキストを更新
    setToolTip(m_fullText);         // ツールチップも同期
    updateElidedText();             // elidedText を再計算して再描画を促す

    // （オプション）変更時にスクロール位置を初期化したい場合：
    // m_offset = 0;
    // stopSlide();
    // startSlideIfNeeded();
}

// 現在のフルテキストを返すゲッター。
// 役割：省略やスクロールの元データとなる完全な文字列をそのまま返す。
// 副作用：なし。
QString ElideLabel::fullText() const { return m_fullText; }

// エリプシス位置（左/中/右）を設定するセッター。
// 役割：Qt::TextElideMode を更新し、現在の幅に合わせて省略文字列を再計算する。
// 注意：はみ出していない場合は見た目に変化はない（m_fullText がそのまま描画される）。
void ElideLabel::setElideMode(Qt::TextElideMode m) {
    if (m_mode == m) return;
    m_mode = m;
    updateElidedText();  // contentsRect().width() に基づいて elidedText を作り直す
}

// 現在のエリプシス位置（Qt::TextElideMode）を返すゲッター。
// 役割：描画/設定の外部参照用。
Qt::TextElideMode ElideLabel::elideMode() const { return m_mode; }

// ホバー中の自動スクロール ON/OFF を設定するセッター。
// 役割：フラグを更新し、現在の状態（ホバー中・はみ出し有無・ドラッグ中か）に応じて
//       即座に開始/停止を判定。
// 注意：ドラッグ中は自動スクロールしない仕様。
void ElideLabel::setSlideOnHover(bool on)
{
    m_slideOnHover = on;
    startSlideIfNeeded();  // 必要ならタイマー起動／停止
}

// 自動スクロールの速度（1ティックあたりの移動ピクセル）を設定。
// 役割：滑らかな見た目や可読性に合わせて速度を調整する。
// 注意：1px 未満は無効化されるため、最低 1 に丸める。
void ElideLabel::setSlideSpeed(int pxPerTick)
{
    m_pxPerTick = qMax(1, pxPerTick);
}

// 自動スクロールのティック間隔（ミリ秒）を設定。
// 役割：タイマーの発火周期を変更する。
// 注意：タイマー稼働中は即時に再スタートして反映。停止中の場合は次回開始時に有効。
void ElideLabel::setSlideInterval(int ms)
{
    m_intervalMs = qMax(1, ms);
    if (m_timer.isActive()) m_timer.start(m_intervalMs);
}

// 下線表示の ON/OFF を設定するセッター。
// 役割：見出しやリンク風の装飾として、ラベル下部に細いラインを描画するかを切り替える。
// 実装：paintEvent で QPalette::Mid の色で 1px ラインを描く。
void ElideLabel::setUnderline(bool on)
{
    m_underline = on;
    update();  // 再描画
}

// 手動パン（ドラッグでの横スクロール）を有効/無効にするセッター。
// 役割：左ドラッグによるスクロール操作の可否を切り替える（mousePress/Move/Release で参照）。
// 実装：フラグ更新後に再描画を要求（下線などの装飾が変わるケースに備える）。
void ElideLabel::setManualPanEnabled(bool on)
{
    m_manualPan = on;
    update();
}

// サイズヒントを返す。
// 役割：高さはフォントに依存、幅はレイアウトに任せる想定で QLabel 既定をそのまま利用。
// 備考：横方向は Expanding、縦方向は Fixed のサイズポリシーを前提にしている。
QSize ElideLabel::sizeHint() const
{
    QSize sz = QLabel::sizeHint();
    // 高さはフォントに合わせて、幅はレイアウト側で決まる想定
    return sz;
}

// エリプシス済みテキスト（m_elidedText）を最新化する。
// 役割：現在のフォントと内容矩形幅（contentsRect().width()）に基づき、
//       m_fullText を elidedText で省略・更新して再描画する。
// 副作用：必要条件を満たす場合は自動スクロール開始を試みる（startSlideIfNeeded）。
void ElideLabel::updateElidedText()
{
    QFontMetrics fm(font());
    m_elidedText = fm.elidedText(m_fullText, m_mode, contentsRect().width());
    update();
    startSlideIfNeeded();
}

// テキストが内容矩形に収まりきらないかを判定する。
// 役割：フルテキスト幅（horizontalAdvance）と描画領域幅を比較し、はみ出しを検出。
// 備考：結果は自動スクロールの開始条件やマウス操作可否の判定に用いられる。
bool ElideLabel::isOverflowing() const
{
    QFontMetrics fm(font());
    return fm.horizontalAdvance(m_fullText) > contentsRect().width();
}

// 自動スクロール用タイマーのタイムアウトハンドラ。
// 役割：一定間隔ごとに表示オフセット m_offset を増分し、update() で再描画を促す。
void ElideLabel::onTimerTimeout()
{
    m_offset += m_pxPerTick; // 1ティックあたりの移動量
    update();                // 再描画要求（paintEvent でオフセットが反映される）
}

// 自動スクロール開始条件の判定と開始処理。
// 役割：ホバー中・テキストはみ出し・非ドラッグ中・自動スクロール許可のすべてを満たすと、
//       タイマーを起動して横スクロールを開始。手動パンが許可されている場合は手のひらカーソルに。
// 注意：既にタイマーが動作中なら再起動しない。条件を満たさない場合は stopSlide() を呼ぶ。
void ElideLabel::startSlideIfNeeded()
{
    if (m_slideOnHover && underMouse() && isOverflowing() && !m_dragging) {
        if (!m_timer.isActive()) {
            m_timer.start(m_intervalMs);
            setCursor(m_manualPan ? Qt::OpenHandCursor : cursor());
        }
    } else {
        stopSlide();
    }
}

// 自動スクロールの停止処理。
// 役割：タイマーを停止し、ドラッグしていない場合はカーソルを矢印に戻す。
// 備考：ドラッグ中はカーソル復帰を行わない（mouseReleaseEvent 側で復帰）。
void ElideLabel::stopSlide()
{
    if (m_timer.isActive()) m_timer.stop();
    if (!m_dragging) setCursor(Qt::ArrowCursor);
}

#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
// ホバー開始イベント（Qt 6 以降）。
// 役割：ホバーに入ったタイミングで自動スクロール開始条件を評価。
void ElideLabel::enterEvent(QEnterEvent*) { startSlideIfNeeded(); }
#else
// ホバー開始イベント（Qt 5 系）。
// 役割：ホバーに入ったタイミングで自動スクロール開始条件を評価。
void ElideLabel::enterEvent(QEvent*) { startSlideIfNeeded(); }
#endif

// ホバー終了イベント。
// 役割：マウスが領域外へ出たら自動スクロールを止め、表示位置（m_offset）を先頭に戻す。
// 注意：オフセットを 0 に戻した後に再描画して、次回表示をリセットした状態にする。
void ElideLabel::leaveEvent(QEvent*)
{
    stopSlide();   // 自動スクロール停止＆カーソル復帰（ドラッグ中でなければ）
    m_offset = 0;  // 次回のスクロール開始位置を先頭に戻す
    update();      // 再描画
}

// リサイズイベント。
// 役割：幅が変わると省略判定が変化するため、elidedText を再計算して表示を更新。
// 注意：基底の QLabel::resizeEvent(e) を先に呼んでから、自前の再計算を行う。
void ElideLabel::resizeEvent(QResizeEvent* e)
{
    QLabel::resizeEvent(e);  // 既定の処理（レイアウト/内部状態更新など）
    updateElidedText();      // 新しい contentsRect().width() を使って省略文字列を更新
}

// マウス押下イベント。
// 役割：手動パンが有効・左クリック・はみ出し有りのとき、ドラッグスクロールを開始する。
// 手順：m_dragging を立て、基準座標（m_lastDragX）を記録、カーソルを握り手に変更、
//       自動スクロールを停止してイベントを消費。
// 注意：条件外（手動パンOFF／右クリック／はみ出し無し）の場合は既定処理へ委譲。
void ElideLabel::mousePressEvent(QMouseEvent* ev)
{
    if (m_manualPan && ev->button() == Qt::LeftButton && isOverflowing()) {
        m_dragging = true;
        m_lastDragX = ev->pos().x();
        setCursor(Qt::ClosedHandCursor);
        stopSlide(); // ドラッグ中は自動スクロール停止
        ev->accept();
        return;
    }
    QLabel::mousePressEvent(ev);
}

// マウス移動イベント（ドラッグ中）。
// 役割：前回位置との差分 dx を用いて表示オフセット m_offset を更新し、水平スクロールさせる。
// 仕様：右へドラッグすると左へスクロール（テキストが左方向へ流れる）。
// 注意：処理後は再描画を要求し、イベントを消費。非ドラッグ時は既定処理へ委譲。
void ElideLabel::mouseMoveEvent(QMouseEvent* ev)
{
    if (m_dragging) {
        int dx = ev->pos().x() - m_lastDragX;
        m_lastDragX = ev->pos().x();
        m_offset = qMax(0, m_offset - dx); // 右へドラッグで左へスクロール
        update();
        ev->accept();
        return;
    }
    QLabel::mouseMoveEvent(ev);
}

// マウス解放イベント。
// 役割：左ボタンのドラッグを終了し、カーソルを復帰。必要なら自動スクロールを再開する。
// 仕様：ホバー中で手動パン有効なら手のひら、そうでなければ矢印カーソルに。
// 注意：左ボタン以外での解放、または非ドラッグ時は既定処理へ委譲。
void ElideLabel::mouseReleaseEvent(QMouseEvent* ev)
{
    if (m_dragging && ev->button() == Qt::LeftButton) {
        m_dragging = false;
        setCursor(underMouse() ? Qt::OpenHandCursor : Qt::ArrowCursor);
        startSlideIfNeeded(); // 必要なら自動スクロール再開
        ev->accept();
        return;
    }
    QLabel::mouseReleaseEvent(ev);
}

// 描画イベント。
// 役割：テキストの表示方式を選択して描画する。
//       1) はみ出し かつ（自動スクロール中 or ドラッグ中）：無限スクロール風にループ描画
//       2) それ以外：幅に収まるならフルテキスト、収まらないならエリプシス（…）で省略描画
// 注意：ループ描画では textW+m_gap を周期とし、m_offset を mod 周期で折り返して継ぎ目を消す。
// 備考：下線オプション（m_underline）時はベースライン直上に 1px ガイドラインを引く。
void ElideLabel::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    const QRect cr = contentsRect();
    QFontMetrics fm(font());
    const int textW = fm.horizontalAdvance(m_fullText);
    // 中央揃え用のベースライン： ascent / descent を考慮して垂直中央に配置
    const int baseY = cr.y() + (cr.height() + fm.ascent() - fm.descent())/2;

    const bool overflow = textW > cr.width();

    if ((m_timer.isActive() || m_dragging) && overflow) {
        // ── 連続ループ描画（スクロール中） ──
        const int startX = cr.x() + 2;      // 左側に少し余白
        const int period = textW + m_gap;   // 1 周期（テキスト幅＋隙間）
        // m_offset を常に [0, period) に正規化（負値対策も含む）
        const int off = (m_offset % period + period) % period;

        // 2 回描画して継ぎ目を目立たなくする
        p.drawText(startX - off,           baseY, m_fullText);
        p.drawText(startX - off + period,  baseY, m_fullText);
    } else {
        // ── 通常描画（非スクロール） ──
        // 収まる：m_fullText、収まらない：m_elidedText（alignment は QLabel 設定を流用）
        p.drawText(cr, static_cast<int>(alignment()) | Qt::AlignVCenter,
                   overflow ? m_elidedText : m_fullText);
    }

    // 下線オプション（装飾）
    if (m_underline) {
        QPen pen(palette().color(QPalette::Mid));
        p.setPen(pen);
        p.drawLine(cr.left(), cr.bottom()-1, cr.right(), cr.bottom()-1);
    }
}
