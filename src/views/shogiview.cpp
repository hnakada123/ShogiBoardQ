#include "shogiview.h"
#include "shogiboard.h"
#include "enginesettingsconstants.h"
#include "elidelabel.h"
#include "globaltooltip.h"
#include "shogigamecontroller.h"

#include <QColor>
#include <QMouseEvent>
#include <QPainter>
#include <QSettings>
#include <QDir>
#include <QApplication>
#include <QHelpEvent>
#include <QFont>
#include <QFontMetrics>
#include <QDebug>
#include <QSizePolicy>
#include <QLayout>
#include <QFontDatabase>
#include <QRegularExpression>

// 角丸(border-radius)を 0px に強制するユーティリティ
static QString ensureNoBorderRadiusStyle(const QString& base)
{
    QString s = base;
    QRegularExpression re(R"(border-radius\s*:\s*[^;]+;?)",
                          QRegularExpression::CaseInsensitiveOption);
    if (re.match(s).hasMatch()) {
        s.replace(re, "border-radius:0px;");
    } else {
        if (!s.isEmpty() && !s.trimmed().endsWith(';')) s.append(';');
        s.append("border-radius:0px;");
    }
    return s;
}

static void enforceSquareCorners(QLabel* lab)
{
    if (!lab) return;
    lab->setStyleSheet(ensureNoBorderRadiusStyle(lab->styleSheet()));
}

Q_LOGGING_CATEGORY(ClockLog, "clock")

using namespace EngineSettingsConstants;

// コンストラクタ
// ・描画/レイアウトに関わる初期値をメンバ初期化子で明示
// ・設定ファイルからマス（square）サイズを読み込み
// ・盤・駒台・時計/名前ラベルを生成し、見た目と挙動を初期化する
ShogiView::ShogiView(QWidget *parent)
    // 親ウィジェットを基底クラスに渡す（Qt のオブジェクト階層に参加させる）
    : QWidget(parent),
    // 直前の処理でエラーが発生したかどうかのフラグ（初期状態はエラーなし）
    m_errorOccurred(false),
    // 盤面の反転モード。0=通常、1=反転 など（初期値は通常）
    m_flipMode(0),
    // マウスの操作モード。クリック選択を有効化
    m_mouseClickMode(true),
    // 局面編集モードの有効/無効（初期は通常対局モード）
    m_positionEditMode(false),
    // ドラッグ中かどうかのフラグ（初期は未ドラッグ）
    m_dragging(false),
    // 将棋盤データ（ボード）へのポインタ（まだ未接続のため nullptr）
    m_board(nullptr),
    // 「駒台からつまんだドラッグ」かどうかのフラグ（初期は盤上から想定）
    m_dragFromStand(false)
{
    // 【入出力の基準パスを明確化】
    // 実行中のアプリケーションのディレクトリをカレントディレクトリに設定。
    // 相対パスでの画像/設定ファイル読み込みを安定させるため。
    QDir::setCurrent(QApplication::applicationDirPath());

    // 【UIスケールの取得】
    // 設定ファイル（INI）からマス（square）のピクセルサイズを読み込む。
    // キー "SizeRelated/squareSize" が無い場合は既定値 50 を採用。
    QSettings settings(SettingsFileName, QSettings::IniFormat);
    m_squareSize = settings.value("SizeRelated/squareSize", 50).toInt();

    // 【レイアウト算出】
    // 盤や駒台など、表示に必要な各種寸法/座標を m_squareSize に基づいて再計算する。
    recalcLayoutParams();
    // 先後の駒台と盤のあいだの水平ギャップを「0.5マスぶん」に設定。
    // 視認性（詰まり感の緩和）とレイアウトの均整をとる目的。
    setStandGapCols(0.5);

    // 【盤面の縦位置調整】
    // ウィジェット上で盤の描画原点を下方向に 20px オフセット。
    // 上部に時計/名前ラベルの「視覚的な呼吸（余白）」を確保する。
    m_offsetY = 20;

    // 【マウストラッキング】
    // ボタン未押下でも mouseMoveEvent を受け取れるようにする。
    // ホバー中のハイライトやドラッグ中の追従描画に必要。
    setMouseTracking(true);

    // ───────────────────────────────── 時計・名前ラベル（先手：黒） ─────────────────────────────────

    // 先手（黒）の時計ラベルを生成し、初期表示を "00:00:00" にする。
    m_blackClockLabel = new QLabel(QStringLiteral("00:00:00"), this);
    // オブジェクト名。スタイルシートやUIテストで特定しやすくするため。
    m_blackClockLabel->setObjectName(QStringLiteral("blackClockLabel"));
    // 表示位置は中央揃え（時計としての視認性を優先）
    m_blackClockLabel->setAlignment(Qt::AlignCenter);
    // マウスイベントをスルー。盤の操作を阻害しないようヒットテスト対象外にする。
    m_blackClockLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    // 背景は透過・文字色は黒。盤や背景との干渉を避けつつ読みやすさを確保。
    m_blackClockLabel->setStyleSheet(QStringLiteral("background: transparent; color: black;"));
    {
        // 時計ラベルのフォントを太字にし、マスサイズに比例して可変。
        // 最低でも 8pt は確保して、小サイズ盤でも可読性を担保。
        QFont f = font();
        f.setBold(true);
        f.setPointSizeF(qMax(8.0, m_squareSize * 0.45));
        m_blackClockLabel->setFont(f);
    }

    // 先手（黒）の名前ラベル（長い名前を省略表示できる特殊ラベル）を生成。
    m_blackNameLabel = new ElideLabel(this);
    // 長いテキストは末尾を省略（…）して収める。
    m_blackNameLabel->setElideMode(Qt::ElideRight);
    // 左寄せ・垂直中央。名前と時計の並びを整える。
    m_blackNameLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    // ホバー時に自動スクロール（スライド）でフルテキストを見せる。
    m_blackNameLabel->setSlideOnHover(true);
    // 手動パンも有効。ドラッグ操作でテキストを左右に移動できる。
    m_blackNameLabel->setManualPanEnabled(true);
    // スクロール速度（大きいほど速い）
    m_blackNameLabel->setSlideSpeed(2);
    // スクロールの更新間隔（ミリ秒）。16ms ≒ 60fps で滑らかに。
    m_blackNameLabel->setSlideInterval(16);

    // ───────────────────────────────── 名前ラベル（後手：白） ─────────────────────────────────

    // 後手（白）の名前ラベルを生成。先手側と同じ見た目/挙動を設定。
    m_whiteNameLabel = new ElideLabel(this);
    m_whiteNameLabel->setElideMode(Qt::ElideRight);
    m_whiteNameLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_whiteNameLabel->setSlideOnHover(true);
    m_whiteNameLabel->setManualPanEnabled(true);
    m_whiteNameLabel->setSlideSpeed(2);
    m_whiteNameLabel->setSlideInterval(16);

    // ───────────────────────────────── 時計ラベル（後手：白） ─────────────────────────────────

    // 後手（白）の時計ラベルを生成し、初期表示を "00:00:00" にする。
    m_whiteClockLabel = new QLabel(QStringLiteral("00:00:00"), this);
    m_whiteClockLabel->setObjectName(QStringLiteral("whiteClockLabel"));
    m_whiteClockLabel->setAlignment(Qt::AlignCenter);
    m_whiteClockLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    m_whiteClockLabel->setStyleSheet(QStringLiteral("background: transparent; color: black;"));
    {
        // 先手側と同じ方針でフォントを設定（太字 + マスサイズ連動 + 最小 8pt）
        QFont f = font();
        f.setBold(true);
        f.setPointSizeF(qMax(8.0, m_squareSize * 0.45));
        m_whiteClockLabel->setFont(f);
    }

    // 【初期配置の反映】
    // 現在のレイアウトパラメータに基づき、時計ラベルの位置・サイズを更新。
    // ここで呼んでおくことで、初回描画前に正しいジオメトリにしておく。
    updateBlackClockLabelGeometry();
    updateWhiteClockLabelGeometry();

    // ★ 起動直後の見た目を整える（両側とも font-weight:400）
    applyStartupTypography();

    // 【イベントフック】
    // 名前ラベルにイベントフィルタを装着。
    // ホバー時のスライド/手動パンの管理や、将来的なツールチップ制御拡張に備える。
    m_blackNameLabel->installEventFilter(this);
    m_whiteNameLabel->installEventFilter(this);
}

// このビューに関連付ける盤データ（ShogiBoard）を設定/差し替えする。
// 役割：
//  - 同一ポインタの再設定は無駄な接続増殖を避けて即 return
//  - 旧ボードがあれば、本ビューへの全シグナル接続を一括解除（重複更新/メモリリーク回避）
//  - 新ボードを保持し、状態変化（dataChanged/boardReset）で update() を起動して再描画を依頼
//  - sizeHint 等が変わる可能性に備え、updateGeometry() で親レイアウトに再配置を要求
void ShogiView::setBoard(ShogiBoard* board)
{
    // 【無駄な再設定の回避】
    // すでに同じ ShogiBoard を保持しているなら処理不要。
    if (m_board == board) return;

    // 【旧接続のクリーンアップ】
    // 以前のボードが存在する場合、そのボード -> このビュー への全接続を解除。
    // QObject::disconnect(m_board, nullptr, this, nullptr) 相当で、安全に差し替え可能にする。
    if (m_board) {
        m_board->disconnect(this);
    }

    // 【差し替え本体】
    // 新しいボード（nullptr 可）を保持。
    m_board = board;

    // 【新接続の確立】
    // 新しいボードが有効なら、表示に影響するイベントで再描画を依頼する。
    if (board) {
        // 盤データが変更されたら、次のイベントループで再描画（update は非同期・差分指向）
        connect(board, &ShogiBoard::dataChanged, this, [this]{ update(); });
        // 盤がリセットされたら、全体を再描画
        connect(board, &ShogiBoard::boardReset,  this, [this]{ update(); });
        // ※ 第3引数に this を渡しているため、このビュー破棄時に自動で接続解除される。
    }

    // 【レイアウト再評価のトリガ】
    // sizeHint() の結果がボードの状態に依存する場合に備え、ジオメトリ更新を通知。
    // 親レイアウトに再レイアウトを促し、必要ならば再配置/再サイズを行う。
    updateGeometry();
}

// イベントフィルタ
// 役割：
//  - 名前ラベル（先手/後手）に対して、標準QToolTipではなくカスタムの GlobalToolTip を用いた
//    遅延生成＆表示制御を行う
//  - ツールチップ要求（QEvent::ToolTip）でフルのベース名を表示、ウィジェット離脱（QEvent::Leave）で非表示
//  - 該当しないオブジェクト/イベントは既定の処理にフォールバック
bool ShogiView::eventFilter(QObject* obj, QEvent* ev)
{
    // 【遅延初期化：ツールチップのシングルトン的利用】
    // パフォーマンスとリソース節約のため、最初の要求時にのみインスタンス化する。
    if (!m_tooltip) {
        m_tooltip = new GlobalToolTip(this);   // 親を this にして破棄を自動化
        m_tooltip->setCompact(true);          // 余白を詰めたコンパクト表示
        m_tooltip->setPointSizeF(12.0);       // ベースとなるフォントサイズ
    }

    // 【対象オブジェクトの限定】
    // 先手/後手の「名前ラベル」に対するイベントのみカスタム処理を行う。
    if (obj == m_blackNameLabel || obj == m_whiteNameLabel) {
        // ツールチップ要求イベント：
        // マウスオーバー位置（グローバル座標）に、フルのベース名を提示する。
        if (ev->type() == QEvent::ToolTip) {
            auto* he = static_cast<QHelpEvent*>(ev); // QEvent -> QHelpEvent への安全なダウンキャスト
            // どちらのラベルかで表示テキスト（ベース名）を切り替える。
            const QString text = (obj == m_blackNameLabel) ? m_blackNameBase : m_whiteNameBase;
            // 画面上のカレントマウス位置（globalPos）付近にカスタムツールチップを表示。
            m_tooltip->showText(he->globalPos(), text);
            return true; // 【重要】標準の QToolTip を抑止し、二重表示/ちらつきを防止
        }
        // ラベルからマウスが離れたらツールチップを隠す（確実に消すためLeaveで制御）
        else if (ev->type() == QEvent::Leave) {
            m_tooltip->hideTip();
        }
    }

    // 【フォールバック】
    // ここで処理しなかったイベントは QWidget 側の既定処理に委譲。
    return QWidget::eventFilter(obj, ev);
}

// 現在ビューに紐づく盤データ（ShogiBoard）を読み出すゲッター。
// nullptr の可能性もある点に注意（未セット時など）。
ShogiBoard* ShogiView::board() const
{
    return m_board;
}

// 1マス（square）の描画サイズ（幅・高さ）を返すゲッター。
// 盤や駒台のレイアウト計算の基本単位として各所で利用される。
QSize ShogiView::fieldSize() const
{
    return m_fieldSize;
}

// 1マスの描画サイズをセットするセッター。
// 役割：値の変化検知 → シグナル発行 → レイアウト再計算/再配置 → 付随UI（時計ラベル）のジオメトリ更新
void ShogiView::setFieldSize(QSize fieldSize)
{
    // 【無駄な再計算回避】 同一サイズなら何もしない。
    if (m_fieldSize == fieldSize) return;

    // 内部状態を更新。
    m_fieldSize = fieldSize;

    // この変更を関心のある外部へ通知（例えばスライダー連動のUIなど）。
    emit fieldSizeChanged(fieldSize);

    // sizeHint() の変化を親レイアウトに伝え、必要ならば再レイアウトを促す。
    updateGeometry();

    // 1マスのサイズが変われば時計ラベルの位置・大きさも影響を受けるため、個別に再配置。
    updateBlackClockLabelGeometry();
    updateWhiteClockLabelGeometry();
}

// レイアウト計算に用いられる推奨サイズ（sizeHint）を返す。
// 親レイアウトがこの値を参考にウィジェットを配置する。
// 盤未設定の場合は最低限の保険値（100x100）を返す。
QSize ShogiView::sizeHint() const
{
    if (!m_board) {
        return QSize(100, 100);
    }

    // 1マスのサイズ
    const QSize squareSize = fieldSize();

    // 全マス分の幅・高さに、左右/上下のオフセット（余白）を加算。
    // m_offsetX, m_offsetY は盤の見た目の「呼吸（余白）」や上部ラベル分の調整に使用。
    int totalWidth  = squareSize.width()  * m_board->files() + m_offsetX * 2;
    int totalHeight = squareSize.height() * m_board->ranks() + m_offsetY * 2;

    return QSize(totalWidth, totalHeight);
}

// 盤状態（反転の有無・段筋数）に基づき、指定された(file, rank)のマスの矩形領域を算出。
// file: 筋（1..m_board->files()）/ rank: 段（1..m_board->ranks()）を前提。
// 返り値はウィジェット座標系（左上原点）の QRect（幅=fs.width(), 高さ=fs.height()）。
QRect ShogiView::calculateSquareRectangleBasedOnBoardState(const int file, const int rank) const
{
    // 盤未設定なら空矩形を返す（呼び出し側で無視できるように）。
    if (!m_board) return QRect();

    const QSize fs = fieldSize();
    int xPosition, yPosition;

    // 【座標系の切替】反転モードのときは上下/左右の向きを入れ替える。
    if (m_flipMode) {
        // 反転時：左から右へ file=1.. の順で配置、縦は上から rank を逆順に敷き詰める。
        xPosition = (file - 1) * fs.width();
        yPosition = (m_board->ranks() - rank) * fs.height();
    } else {
        // 通常時：右から左へ file を反転配置、縦は下から上へ rank=1.. の順で配置。
        xPosition = (m_board->files() - file) * fs.width();
        yPosition = (rank - 1) * fs.height();
    }

    // 指定マス一個分の矩形を返す（位置：x/y, サイズ：fs）。
    return QRect(xPosition, yPosition, fs.width(), fs.height());
}

// 段番号/筋番号などのラベル描画に用いる矩形領域を算出。
// 盤の反転状態に合わせてYのみ上下を切替。Xは左→右の自然順で配置し、最後にUI都合で +30px 平行移動。
// ※ +30px はラベルの視認性/余白確保のための固定オフセット（デザイン調整値）。
QRect ShogiView::calculateRectangleForRankOrFileLabel(const int file, const int rank) const
{
    if (!m_board) return QRect();

    const QSize fs = fieldSize();

    // ラベルは左→右の自然順で扱うため X は (file - 1) を素直に採用。
    int adjustedX = (file - 1) * fs.width();

    int adjustedY;
    if (m_flipMode) {
        // 反転時は段を逆順に変換（盤面の上下関係に合わせる）
        adjustedY = (m_board->ranks() - rank) * fs.height();
    } else {
        // 通常時は rank=1 を最上段（0オリジン）とする
        adjustedY = (rank - 1) * fs.height();
    }

    // ラベル領域の基本矩形を作り、UI上の意図的な余白として X に +30px 平行移動して返す。
    return QRect(adjustedX, adjustedY, fs.width(), fs.height()).translated(30, 0);
}

// 駒タイプと描画用アイコン（QIcon）を関連付けて登録する。
// 役割：駒文字（例：'P','L','N','B','R','G','S','K','p',...）に対応するアイコンをセットし、再描画を依頼。
// 備考：update() は非同期に再描画イベントを発行（即座にpaintEventを呼ばない）。
void ShogiView::setPiece(char type, const QIcon &icon)
{
    // 駒タイプ→アイコン のマップに挿入/上書き
    m_pieces.insert(type, icon);

    // 駒画像が更新されたのでビューの再描画を要求
    update();
}

// 駒タイプに対応するアイコンを取得するゲッター。
// 見つからない場合はデフォルト構築の QIcon（空）を返す。
// 役割：描画時に、盤上/駒台の駒アイコンを取り出すのに使用。
QIcon ShogiView::piece(QChar type) const
{
    return m_pieces.value(type, QIcon());
}

// 段番号（rank）の描画エントリポイント。
// 役割：
//  - 盤が未設定なら何もしない安全弁
//  - 共通の描画状態（ここでは文字色＝QPalette::WindowText）を一度だけ設定
//  - 1..ranks() を走査して各段の描画を drawRank() に委譲
// 前提：drawRank() は QPainter の永続状態（ペン/ブラシ/変換/クリップ等）を汚さないこと。
//       もし一時的に変更する場合は drawRank() 内部で局所 save()/restore() を使う。
void ShogiView::drawRanks(QPainter* painter)
{
    // 【安全弁】盤が無ければ段ラベルは描けない
    if (!m_board) return;

    // 【共通状態設定】段ラベル用の文字色（パレットに従う）
    painter->setPen(palette().color(QPalette::WindowText));

    // 【描画ループ】1段目から最終段まで順に描画
    for (int r = 1; r <= m_board->ranks(); ++r) {
        drawRank(painter, r);
    }
}

// 筋番号（file）の描画エントリポイント。
// 役割：
//  - 盤が未設定なら何もしない安全弁
//  - 共通の描画状態（文字色など）を一度だけ設定
//  - 1..files() を走査して各筋の描画を drawFile() に委譲
// 備考：フォントサイズ等を変更する場合は、ここでまとめて setFont しておくと効率的。
// 前提：drawFile() は QPainter の永続状態を汚さない（必要時のみ局所 save()/restore()）。
void ShogiView::drawFiles(QPainter* painter)
{
    // 【安全弁】盤が無ければ筋ラベルは描けない
    if (!m_board) return;

    // 【共通状態設定】筋ラベル用の文字色（パレットに従う）
    painter->setPen(palette().color(QPalette::WindowText));

    // 例：フォントサイズを調整したい場合（必要になったら有効化）
    // QFont f = painter->font();
    // f.setPointSize(...)
    // painter->setFont(f);

    // 【描画ループ】1筋目から最終筋まで順に描画
    for (int c = 1; c <= m_board->files(); ++c) {
        drawFile(painter, c);
    }
}

// 盤の各マス（field）を描画するエントリポイント。
// 最適化方針を適用：セルごとの save()/restore() を撤去し、共通状態は外側で一度だけ設定。
// 前提：drawField() は QPainter の永続状態（ペン/ブラシ/変換/クリップ等）を汚さないこと。
// 　　（もし一時的に変更する必要がある場合は、drawField() 内でその箇所だけ局所的に save()/restore() を行う）
void ShogiView::drawBoardFields(QPainter* painter)
{
    // 【安全弁】盤が未設定なら何もしない
    if (!m_board) return;

    // 【共通状態の一括設定】
    // 例：マスの境界線などで使うペン色を一度だけセットして、以降は drawField() が恒久変更しない前提にする。
    // ※ 個々のマスでブラシ（塗り）を切り替える場合、ブラシは drawField() 内で一時的に設定する。
    painter->setPen(palette().color(QPalette::Dark));
    // 必要に応じて他の共通設定もここで行う：
    // painter->setRenderHint(QPainter::Antialiasing, false);
    // painter->setBrush(Qt::NoBrush);

    // 【描画ループ】段（r）× 筋（c）で全マスを走査し、個々の描画は drawField() に委譲
    for (int r = 1; r <= m_board->ranks(); ++r) {
        for (int c = 1; c <= m_board->files(); ++c) {
            // セルごとの save/restore は行わない（コスト削減）。
            // drawField() 側は必要箇所のみ局所 save/restore で自己完結させる契約。
            drawField(painter, c, r);
        }
    }
}

void ShogiView::drawBlackNormalModeStand(QPainter* painter)
{
    painter->setPen(palette().color(QPalette::Dark));
    // ★ r=3..9 → r=6..9（4行）に短縮
    for (int r = 6; r <= 9; ++r) {
        for (int c = 1; c <= 2; ++c) {
            drawBlackStandField(painter, c, r);
        }
    }
}

void ShogiView::drawWhiteNormalModeStand(QPainter* painter)
{
    painter->setPen(palette().color(QPalette::Dark));
    // ★ r=1..7 → r=1..4（4行）に短縮
    for (int r = 1; r <= 4; ++r) {
        for (int c = 1; c <= 2; ++c) {
            drawWhiteStandField(painter, c, r);
        }
    }
}

// 通常対局モードにおける「駒台」描画の統括エントリポイント。
// 役割：先手（黒）→ 後手（白）の順に、通常モード用の駒台マスを描画する関数へ委譲する。
// 方針：共通の描画状態（ペン/ブラシ等）の設定は各サブ関数側で一括設定済みとし、ここでは状態を変更しない。
// 前提：drawBlackNormalModeStand()/drawWhiteNormalModeStand() は QPainter の永続状態を汚さない
//      （必要箇所のみ局所 save()/restore() を用いる契約）。
// 備考：描画順は重なり順（zオーダ）や視覚効果に影響し得るため、UI要件に応じて入れ替え可能。
void ShogiView::drawNormalModeStand(QPainter* painter)
{
    // 先手（黒）側の通常モード駒台を描画
    drawBlackNormalModeStand(painter);

    // 後手（白）側の通常モード駒台を描画
    drawWhiteNormalModeStand(painter);
}

// 盤上の全ての駒を描画するエントリポイント。
// 最適化方針：セル（マス）ごとの save()/restore() を撤去し、共通状態は外側で必要に応じて一度だけ設定。
// 前提：drawPiece() は QPainter の永続状態（ペン/ブラシ/変換/クリップ等）を汚さないこと。
//       もし一時的な状態変更（座標変換やクリッピング等）が必要な場合は、drawPiece() 内で
//       その箇所だけ局所的に save()/restore() を行う契約とする。
// 備考：段は ranks() から 1 へ向かって降順に走査。
//       （必要に応じて、重なり順やドロップシャドウなどの視覚効果に合わせて順序を変えられる）
void ShogiView::drawPieces(QPainter* painter)
{
    // 【安全弁】盤が未設定なら何もしない
    if (!m_board) return;

    // 【共通状態の一括設定（必要なら）】
    // 駒アイコンをスケーリング描画する際の画質を上げたい場合に有効化。
    // painter->setRenderHint(QPainter::SmoothPixmapTransform, true);

    // 【描画ループ】段（r）を降順、筋（c）を昇順に走査し、各マスの駒を描画
    for (int r = m_board->ranks(); r > 0; --r) {
        for (int c = 1; c <= m_board->files(); ++c) {
            // セルごとの save/restore は行わない（コスト削減）。
            // drawPiece() 側は必要箇所のみ局所保護する契約。
            drawPiece(painter, c, r);

            // 【エラーハンドリング】
            // 描画中に致命的な条件が発生した場合（フォント/画像読み込み失敗など想定）、
            // フラグを見て素早く中断。セル単位の save() を使っていないため、
            // ここでの早期 return でも painter 状態リークの懸念はない。
            if (m_errorOccurred) return;
        }
    }
}

// 【通常対局モード：先手（左側）駒台のアイコンを 4×2 で描画】
void ShogiView::drawPiecesBlackStandInNormalMode(QPainter* painter)
{
    // 左列：R, G, N, P （= 飛, 金, 桂, 歩）
    // 右列：K, B, S, L （= 王, 角, 銀, 香）
    const int ranks [4] = { 6, 7, 8, 9 };

    // 左列は file=2、右列は file=1（このコードの座標系では file=2 の方が左寄り）
    for (int row = 0; row < 4; ++row) {
        for (int file = 1; file <= 2; ++file) {
            drawBlackStandPiece(painter, file, ranks[row]);
        }
    }
}

// 【通常対局モード：後手（右側）駒台のアイコンを 4×2 で描画】
void ShogiView::drawPiecesWhiteStandInNormalMode(QPainter* painter)
{
    // 左列：r, g, n, p（= 飛, 金, 桂, 歩）
    // 右列：k, b, s, l（= 王, 角, 銀, 香）
    const int ranks [4] = { 4, 3, 2, 1 };

    // 右側駒台でも相対的な“左列”は file=2、“右列”は file=1 が自然に並ぶ
    for (int row = 0; row < 4; ++row) {
        for (int file = 1; file <= 2; ++file) {
            drawWhiteStandPiece(painter, file, ranks[row]);
        }
    }
}

void ShogiView::drawPiecesStandFeatures(QPainter* painter)
{
    // 先手/後手の駒台にある「駒」と「枚数」を描画
    drawPiecesBlackStandInNormalMode(painter);
    drawPiecesWhiteStandInNormalMode(painter);
}

// 画面全体の描画エントリポイント（paintEvent）。
// 方針：
//  - 最適化後の契約に基づき、セルごとの save()/restore() は各描画関数の内部で
//    「必要箇所のみ局所的に」行う（外側での大量反復はしない）。
//  - ここでは共通のレンダリングヒント等を一度だけ設定し、以降のサブ関数は
//    QPainter の永続状態（ペン/ブラシ/変換/クリップ）を汚さない前提で呼び出す。
//  - 描画順序は背面→前面のレイヤ構造を明確にし、重ね合わせを安定化させる。
void ShogiView::paintEvent(QPaintEvent *)
{
    // 【安全弁】盤未設定、またはエラーフラグが立っている場合は描画を行わない。
    if (!m_board || m_errorOccurred) return;

    // 【ペインタ開始】このスコープでのみ QPainter を有効化。
    QPainter painter(this);

    // 【共通描画状態の一括設定】
    // 盤の格子線などはジャギーの少ない直線を優先するためアンチエイリアスは基本OFF。
    // テキストは可読性重視でON、駒アイコンの拡縮品質を上げるため SmoothPixmapTransform をON。
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    // 【描画順序：背面 → 前面】
    // 1) 盤面（マスの背景・枠など）
    drawBoardFields(&painter);

    // 2) 局面編集/通常に応じた周辺（駒台グリッドなどのフィールド）
    drawNormalModeStand(&painter);

    // 3) 盤の星（目印）
    drawFourStars(&painter);

    // 4) ハイライト（選択/移動可能マスなど）
    drawHighlights(&painter);

    // 5) 盤上の駒
    drawPieces(&painter);

    // 描画中に致命的な異常が検知された場合はここで打ち切る。
    if (m_errorOccurred) return;

    // 6) 先手/後手の駒台にある「駒」と「枚数」を描画
    drawPiecesStandFeatures(&painter);

    // 7) 段・筋ラベル（最前面に近いレイヤに載せる）
    drawRanks(&painter);
    drawFiles(&painter);

    // 8) 最前面：ドラッグ中の駒（マウス追従）。盤やラベルより上に重ねる。
    //    ※ 重要：drawDraggingPiece() の内部で新たに QPainter を begin しないこと。
    //            （この paintEvent 中は既に painter がアクティブなため競合する）
    drawDraggingPiece(&painter);
}

// 【ドラッグ中の駒を描画】（最適化適用版）
// 方針：paintEvent で開始済みの QPainter を使い回すため、ここで新たに QPainter を生成しない。
// 前提：本関数は QPainter の永続状態（ペン/ブラシ/変換/クリップ等）を汚さない。
// 備考：ドラッグ位置 m_dragPos の中心に、1マス分（squareSize）でアイコンを描画する。
void ShogiView::drawDraggingPiece(QPainter* painter)
{
    // 【前提確認】ドラッグ中でなければ何もしない／駒種が空白なら描かない
    if (!m_dragging || m_dragPiece == ' ') return;

    // 【アイコン取得】該当駒のアイコンが無ければ描かない（安全弁）
    const QIcon icon = piece(QChar(m_dragPiece));
    if (icon.isNull()) return;

    // 【描画矩形算出】ドラッグ座標を矩形の中心に据える（正方形：1マス）
    const int s = squareSize();
    const QRect r(m_dragPos.x() - s / 2, m_dragPos.y() - s / 2, s, s);

    // 【描画】既存の painter を用いて中央揃えでペイント（状態は汚さない）
    icon.paint(painter, r, Qt::AlignCenter);
}

// 将棋盤の「四隅の星（3,3）（6,3）（3,6）（6,6）」を描画する。
// 最適化方針：この関数内でのみブラシ/ペン等の状態を一時変更し、終了時に必ず元へ戻す（局所 save/restore）。
// 役割：盤面上の 4 箇所に小さな点（塗りつぶし円）を打ち、視覚的な基準点を提供する。
// 座標系：各星の中心は「マスサイズ×(3 or 6) ＋ オフセット(m_offsetX/Y)」で求める。
void ShogiView::drawFourStars(QPainter* painter)
{
    // 【状態の局所保護】このブロックでのみ描画状態を変更し、外へ影響させない
    painter->save();

    // 【描画スタイル】星は塗りつぶし円で表現（縁取り色は現状維持：必要なら NoPen を設定）
    painter->setBrush(palette().color(QPalette::Dark));
    // 例：縁取りを消したい場合は以下を有効化
    // painter->setPen(Qt::NoPen);

    // 【サイズ/基準点】
    const int starRadius = 4;             // 星（点）の半径（px）。必要に応じてマス比率に連動させてもよい。
    const int basePoint3 = m_squareSize * 3; // 3マス分
    const int basePoint6 = m_squareSize * 6; // 6マス分

    // 【描画】（x, y）はウィジェット座標。盤の原点シフトに m_offsetX/Y を加味。
    painter->drawEllipse(QPoint(basePoint3 + m_offsetX, basePoint3 + m_offsetY), starRadius, starRadius);
    painter->drawEllipse(QPoint(basePoint6 + m_offsetX, basePoint3 + m_offsetY), starRadius, starRadius);
    painter->drawEllipse(QPoint(basePoint3 + m_offsetX, basePoint6 + m_offsetY), starRadius, starRadius);
    painter->drawEllipse(QPoint(basePoint6 + m_offsetX, basePoint6 + m_offsetY), starRadius, starRadius);

    // 【状態復元】外側の描画に影響を残さない
    painter->restore();
}

// 盤の左端（X座標, px）を返すゲッター。
// 役割：盤の描画/ヒットテスト/ラベル配置の基準となる「左側の外側境界」を返す。
// 備考：反転（m_flipMode）の有無に関わらず、盤の左端Xは m_offsetX で一定。
int ShogiView::boardLeftPx() const { return m_offsetX; }

// 盤の右端（X座標, px）を返すゲッター。
// 役割：盤の横幅（files × 1マス幅）を左端に加算し、右側の外側境界（exclusive）を返す。
//  - files は盤の筋数。盤未設定時は将棋の標準 9 筋を仮定（安全なフォールバック）。
//  - 戻り値は「右端の外側境界（exclusive）」：左端 <= x < 右端 の右端に相当。
//    ヒットテスト等で矩形幅を計算する際に扱いやすい表現。
int ShogiView::boardRightPx() const {
    const int files = m_board ? m_board->files() : 9;                 // 未設定時は 9×9 を前提
    return m_offsetX + m_squareSize * files;                           // 左端 + 盤の総幅
}


// 指定された (file, rank) のマス（1 マス分の矩形）を描画する。
// 最適化方針：本関数内でのみペン/ブラシ状態を一時変更するため、局所的に save()/restore() を行う。
// 役割：
//  - 盤の反転状態も考慮したマス矩形を算出（calculateSquareRectangleBasedOnBoardState）
//  - 盤の描画原点シフト（m_offsetX/m_offsetY）を加味して実座標へ変換
//  - 木目風の塗り色と枠線色で 1 マスを描画
void ShogiView::drawField(QPainter* painter, const int file, const int rank) const
{
    // 【盤座標 → ウィジェット座標】
    const QRect fieldRect = calculateSquareRectangleBasedOnBoardState(file, rank);
    QRect adjustedRect(fieldRect.left() + m_offsetX,
                       fieldRect.top()  + m_offsetY,
                       fieldRect.width(),
                       fieldRect.height());

    // 【状態の局所保護】このマスの描画で変更するペン/ブラシを外に漏らさない
    painter->save();

    // 【描画スタイル】
    // マスの塗り（木目系の色味）。必要に応じてパレット依存や明度調整に置き換え可。
    const QColor fillColor(222, 196, 99, 255);
    painter->setBrush(fillColor);

    // マスの枠線色（やや暗いグレー）。デザインポリシーに合わせて QPalette::Dark も可。
    painter->setPen(QColor(30, 30, 30));

    // 【描画本体】
    painter->drawRect(adjustedRect);

    // 【状態復元】以降の描画に影響を残さない
    painter->restore();
}

// 駒台セル（1マス）の描画矩形を算出するユーティリティ。
// 役割：
//  - 盤上の基準マス矩形（fieldRect）から、先手/後手の駒台側に水平オフセットした矩形を返す。
//  - 盤の反転状態（flip）と、どちら側の駒台か（leftSide）に応じて、左右どちらへ寄せるかを切り替える。
// 引数：
//  - flip    : 盤の反転状態。true=反転（上下左右が入れ替わる前提）、false=通常。
//  - param   : 駒台セルを盤の基準矩形からどれだけ水平方向にずらすか（px, 正値）。
//  - offsetX : 盤全体のX方向オフセット（描画原点シフト）。
//  - offsetY : 盤全体のY方向オフセット（描画原点シフト）。
//  - fieldRect : 盤の基準マス矩形（この左上・サイズを基準に駒台セルを配置する）。
//  - leftSide  : 「左側の駒台か」を示すフラグ。true=左側、false=右側。
// 戻り値：調整後（オフセット適用後）の駒台セル矩形。
// 視点：
//  - flip==true のとき「先手は左／後手は右」へ寄せる（左右関係が反転）
//  - flip==false のとき「先手は右／後手は左」へ寄せる（通常の左右関係）
static inline QRect makeStandCellRect(bool flip, int param, int offsetX, int offsetY, const QRect& fieldRect, bool leftSide)
{
    QRect adjustedRect;

    if (flip) {
        // 【反転時】先手は左、後手は右に配置。
        // leftSide==true（左側の駒台）のときは基準から -param、右側は +param 側へシフト。
        adjustedRect.setRect(fieldRect.left() + (leftSide ? -param : +param) + offsetX,
                             fieldRect.top()  + offsetY,
                             fieldRect.width(),
                             fieldRect.height());
    } else {
        // 【通常時】先手は右、後手は左に配置。
        // leftSide==true（左側の駒台）のときは基準から +param、右側は -param 側へシフト。
        adjustedRect.setRect(fieldRect.left() + (leftSide ? +param : -param) + offsetX,
                             fieldRect.top()  + offsetY,
                             fieldRect.width(),
                             fieldRect.height());
    }

    return adjustedRect;
}

// 先手（黒）側の駒台セル（1マス）を描画する。
// 最適化方針：この関数内のみでペン/ブラシを一時変更するため、局所的に save()/restore() を行う。
// 手順：
//  1) 盤の(file, rank)に対応する基準マス矩形を取得（反転含む）
//  2) 基準マスから「左側の駒台」方向へ水平シフトして駒台セル矩形を算出
//  3) 木目系の塗り色で矩形を描画（枠線も同色で目立たせない）
void ShogiView::drawBlackStandField(QPainter* painter, const int file, const int rank) const
{
    // (1) 盤座標→基準マス矩形
    const QRect fieldRect = calculateSquareRectangleBasedOnBoardState(file, rank);

    // (2) 先手は「左側の駒台」へ寄せる想定。m_param1 は水平シフト量（px）。
    QRect adjustedRect = makeStandCellRect(
        m_flipMode,               // 反転有無
        m_param1,                 // 水平方向の寄せ量（先手用パラメータ）
        m_offsetX, m_offsetY,     // 盤全体の原点シフト
        fieldRect,
        /*leftSide=*/true         // 左側の駒台へ配置
        );

    // (3) 状態の局所保護と描画
    painter->save();

    const QColor fillColor(228, 167, 46, 255); // 駒台の塗り（木目系）
    painter->setPen(fillColor);                // 枠線も同色にして目立たせない
    painter->setBrush(fillColor);
    painter->drawRect(adjustedRect);

    painter->restore();
}

// 後手（白）側の駒台セル（1マス）を描画する。
// 最適化方針：本関数内のみで状態を変更し、終了時に元へ戻す（局所 save()/restore()）。
// 手順：基準マス→「右側の駒台」方向へ水平シフト→木目系の塗りで矩形描画。
void ShogiView::drawWhiteStandField(QPainter* painter, const int file, const int rank) const
{
    // 盤座標→基準マス矩形
    const QRect fieldRect = calculateSquareRectangleBasedOnBoardState(file, rank);

    // 後手は「右側の駒台」へ寄せる想定。m_param2 は後手用の水平シフト量（px）。
    QRect adjustedRect = makeStandCellRect(
        m_flipMode,               // 反転有無
        m_param2,                 // 水平方向の寄せ量（後手用パラメータ）
        m_offsetX, m_offsetY,     // 盤全体の原点シフト
        fieldRect,
        /*leftSide=*/false        // 右側の駒台へ配置
        );

    // 状態の局所保護と描画
    painter->save();

    const QColor fillColor(228, 167, 46, 255); // 駒台の塗り（木目系）
    painter->setPen(fillColor);                // 枠線も同色に
    painter->setBrush(fillColor);
    painter->drawRect(adjustedRect);

    painter->restore();
}

// 指定された (file, rank) のマスに存在する「駒」を描画する。
// 方針：この関数では QPainter の永続状態（ペン/ブラシ/変換/クリップ等）を変更しない。
//       アイコン描画のみを行うため、save()/restore() は不要（最適化適用）。
// 前提：呼び出し側（例：drawPieces）が m_board の非 nullptr を保証している。
void ShogiView::drawPiece(QPainter* painter, const int file, const int rank)
{
    // 【ドラッグ中の元マスは描かない】
    // ドラッグ中の駒は最前面で drawDraggingPiece() により別途描画するため、
    // 元いたマスの駒は二重描画を避ける目的でスキップする。
    if (m_dragging && file == m_dragFrom.x() && rank == m_dragFrom.y()) {
        return;
    }

    // 【盤座標 → ウィジェット座標】
    // 反転状態を考慮したマス矩形を取得し、盤の原点シフト（m_offsetX/Y）を加味して実座標へ。
    const QRect fieldRect = calculateSquareRectangleBasedOnBoardState(file, rank);
    QRect adjustedRect(fieldRect.left() + m_offsetX,
                       fieldRect.top()  + m_offsetY,
                       fieldRect.width(),
                       fieldRect.height());

    // 【盤から駒種を取得】
    QChar value = m_board->getPieceCharacter(file, rank);

    // 【アイコン描画】
    // 空白でなければ駒に対応する QIcon を取得し、存在する場合のみ中央揃えで描く。
    if (value != ' ') {
        const QIcon icon = piece(value);
        if (!icon.isNull()) {
            // SmoothPixmapTransform は paintEvent 側の共通設定で有効化済みを想定。
            icon.paint(painter, adjustedRect, Qt::AlignCenter);
        }
    }
}

/**
 * @brief 駒台セル内に「等倍」の駒アイコンを重ね描きする（最大2段＝最大3枚表示）。
 *
 * @param painter       描画先 QPainter（外側で begin() されている前提）
 * @param adjustedRect  駒台の 1 セル矩形（この矩形で clip してセル外は不可視）
 * @param value         駒種キー（piece(value) から QIcon を取得）
 *
 * @details
 * - アイコンはセル等倍（拡大縮小なし）。重ね全体の“中心”が常にセル中心に来るよう配置します。
 * - 枚数に応じて「後ろ（奥）」の駒ほど左にオフセットして重ねます。
 * - 見せる重なりは最大2段（= 表示するアイコンは最大3枚）。4枚目以降は描画せず、バッジのみ総数表示。
 * - 右下の枚数バッジは自動的にサイズ調整し、必要に応じて基準位置を少し左へ寄せてセル内に収めます。
 *
 * @note 調整パラメータ
 *   - maxOverlapSteps … 重なり段数の上限（既定 2）。表示最大枚数は maxOverlapSteps+1。
 *   - perStep         … 1 段あたりの横オフセット量（アイコン幅に対する比率、既定 0.05）。
 *   - hardMax         … 左方向への総広がりの上限（見栄え用、既定 iconW * 0.85）。
 */
void ShogiView::drawStandPieceIcon(QPainter* painter, const QRect& adjustedRect, QChar value) const
{
    const int count = (m_dragging && m_tempPieceStandCounts.contains(value))
    ? m_tempPieceStandCounts[value]
    : m_board->m_pieceStand.value(value);
    if (count <= 0 || value == QLatin1Char(' ')) return;

    const QIcon icon = piece(value);

    // === 等倍（セルいっぱい） ===
    const int cellW = adjustedRect.width();
    const int cellH = adjustedRect.height();
    const int iconW = cellW;
    const int iconH = cellH;

    // ▼ここで“見せる重なり段数”と“最大表示枚数”をハードキャップ
    const int maxOverlapSteps = 2;                 // 最大重なり段数（= 左への段差は2段まで）
    const int maxVisibleIcons = maxOverlapSteps + 1; // 最大表示枚数（前面含め最大3枚）
    const int visible = qMin(count, maxVisibleIcons);
    const int stepsEff = qMax(0, visible - 1);     // 0..5

    // 重なり幅（横広がり）。※必要なら係数だけ調整（例: 0.05）
    const qreal perStep = iconW * 0.05;
    const qreal hardMax = iconW * 0.85;
    const qreal totalSpread = qMin(hardMax, perStep * qreal(stepsEff));

    // 最前面ベースを「重ね全体センター=マス中心」に配置
    QRect base(0, 0, iconW, iconH);
    base.moveCenter(QPoint(adjustedRect.center().x() + int(totalSpread / 2.0),
                           adjustedRect.center().y()));

    // バッジ右端のはみ出しを微修正
    int margin = qMax(2, iconW / 40);
    int overflowRight = base.right() - (adjustedRect.right() - margin);
    if (overflowRight > 0) base.translate(-overflowRight, 0);

    painter->save();
    painter->setClipRect(adjustedRect);
    painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter->setRenderHint(QPainter::Antialiasing, true);

    QPixmap pm = icon.pixmap(QSize(iconW, iconH), QIcon::Normal, QIcon::On);
    const bool iconOk = !pm.isNull();

    // 奥(左)→手前(右)。表示は最大 visible 枚に限定（4枚目以降は描かない）
    for (int i = 0; i < visible; ++i) {
        const qreal t = (stepsEff > 0) ? (qreal(i) / qreal(stepsEff)) : 1.0; // 0..1
        const qreal shiftX = -totalSpread * (1.0 - t);                        // -spread..0
        const QRect r = base.translated(int(shiftX), 0);

        if (iconOk) painter->drawPixmap(r, pm);
        else {
            QFont f = painter->font();
            f.setPixelSize(qMax(8, int(iconH * 0.7)));
            painter->setFont(f);
            painter->drawText(r, Qt::AlignCenter, QString(value));
        }
    }

    // 右下バッジ（総数表示）はそのまま
    if (count >= 2) {
        const QRect topRect = base;
        QFont f = painter->font();
        int px = qBound(9, int(iconH * 0.34), 48);
        f.setPixelSize(px);
        f.setBold(true);
        painter->setFont(f);

        const QString text = QString::number(count);
        QFontMetrics fm(f);
        int padX = qMax(3, iconW / 18);
        int padY = qMax(2, iconH / 22);
        QSize tsz = fm.size(Qt::TextSingleLine, text);

        const int maxBadgeW = qMax(8, iconW - 2 * margin);
        const int maxBadgeH = qMax(8, iconH - 2 * margin);
        const int needW = tsz.width() + padX * 2;
        const int needH = tsz.height() + padY * 2;
        const qreal scale = qMin<qreal>(1.0, qMin(qreal(maxBadgeW)/needW, qreal(maxBadgeH)/needH));
        if (scale < 1.0) {
            px   = qMax(8, int(px   * scale));
            padX = qMax(2, int(padX * scale));
            padY = qMax(2, int(padY * scale));
            f.setPixelSize(px);
            painter->setFont(f);
            fm  = QFontMetrics(f);
            tsz = fm.size(Qt::TextSingleLine, text);
        }

        QRect badge(0, 0, qMin(maxBadgeW, tsz.width() + padX * 2),
                    qMin(maxBadgeH, tsz.height() + padY * 2));
        badge.moveBottomRight(QPoint(topRect.right() - margin, topRect.bottom() - margin));

        const qreal radius = qMin(badge.width(), badge.height()) * 0.35;
        QPen outline(QColor(255, 255, 255, 220), qMax(1, iconW / 60));
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(0, 0, 0, 170));
        painter->drawRoundedRect(badge, radius, radius);

        painter->setPen(outline);
        painter->setBrush(Qt::NoBrush);
        painter->drawRoundedRect(badge, radius, radius);

        painter->setPen(Qt::white);
        painter->drawText(badge, Qt::AlignCenter, text);
    }

    painter->restore();
}

// 先手（黒）側の名前ラベル（ElideLabel）を返すゲッター。
// 役割：テキスト設定・ツールチップ・スクロール挙動（スライド）等の制御に利用。
// 備考：本ビュー（this）を親にもつ子ウィジェットへの生ポインタを返す（所有権は移動しない）。
ElideLabel* ShogiView::blackNameLabel() const { return m_blackNameLabel; }

// 先手（黒）側の時計ラベル（QLabel）を返すゲッター。
// 役割：残り時間表示・フォント/スタイル調整・配置制御に利用。
// 備考：所有権は本ビュー側。呼び出し側で delete 等は不要。
QLabel* ShogiView::blackClockLabel() const { return m_blackClockLabel; }

// 後手（白）側の名前ラベル（ElideLabel）を返すゲッター。
// 役割：長い名前の省略表示（エリプシス）、ホバー時スライド、手動パン等の制御に利用。
ElideLabel* ShogiView::whiteNameLabel() const { return m_whiteNameLabel; }

// 後手（白）側の時計ラベル（QLabel）を返すゲッター。
// 役割：残り時間の描画・スタイル変更・レイアウト調整に利用。
// 注意：いずれのゲッターもヌルを返さない想定（コンストラクタで生成済み）。
QLabel* ShogiView::whiteClockLabel() const { return m_whiteClockLabel; }

// 先手（黒）側の駒台セルに対応する「駒アイコン」を描画する。
// 方針：この関数自身は QPainter の永続状態（ペン/ブラシ/変換/クリップ等）を変更しない。
// 手順：
//  1) (file, rank) に対応する基準マス矩形を取得（反転状態を考慮）
//  2) 基準マスから「左側の駒台」方向へ水平シフトして、駒台セルの描画矩形を算出
//  3) rank を先手用の駒種にマッピング（rankToBlackShogiPiece）
//  4) 枚数/ドラッグ一時枚数を考慮する描画ヘルパ（drawStandPieceIcon）に委譲
void ShogiView::drawBlackStandPiece(QPainter* painter, const int file, const int rank) const
{
    // (1) 盤座標 → 基準マス矩形
    const QRect fieldRect = calculateSquareRectangleBasedOnBoardState(file, rank);

    // (2) 先手は「左側の駒台」へ寄せる想定。m_param1 は水平シフト量（px）。
    QRect adjustedRect = makeStandCellRect(
        m_flipMode,               // 反転有無
        m_param1,                 // 先手側の寄せ量
        m_offsetX, m_offsetY,     // 盤全体の原点シフト
        fieldRect,
        /*leftSide=*/true         // 左側の駒台へ配置
        );

    // (3) 段番号→先手の駒種（駒文字）へのマッピング
    //     例：歩・香・桂・銀・金・角・飛・玉…などの並びを rank に応じて返す想定。
    QChar value = rankToBlackShogiPiece(file, rank);

    // (4) 駒台セル内へアイコン描画（枚数 0 なら描かない、ドラッグ中は一時枚数を優先）
    //     ※ drawStandPieceIcon は painter の状態を汚さない前提。
    drawStandPieceIcon(painter, adjustedRect, value);
}

// 後手（白）側の駒台セルに対応する「駒アイコン」を描画する。
// 方針：この関数自身は QPainter の永続状態（ペン/ブラシ/変換/クリップ等）を変更しない。
// 手順：
//  1) (file, rank) に対応する基準マス矩形を取得（反転状態を考慮）
//  2) 基準マスから「右側の駒台」方向へ水平シフトして、駒台セルの描画矩形を算出
//  3) rank を後手用の駒種にマッピング（rankToWhiteShogiPiece）
//  4) 枚数/ドラッグ一時枚数を考慮する描画ヘルパ（drawStandPieceIcon）に委譲
void ShogiView::drawWhiteStandPiece(QPainter* painter, const int file, const int rank) const
{
    // (1) 盤座標 → 基準マス矩形（反転モード m_flipMode を内部で考慮）
    const QRect fieldRect = calculateSquareRectangleBasedOnBoardState(file, rank);

    // (2) 後手は「右側の駒台」へ寄せる想定。m_param2 は後手用の水平シフト量（px）。
    QRect adjustedRect = makeStandCellRect(
        m_flipMode,               // 反転有無
        m_param2,                 // 後手側の寄せ量
        m_offsetX, m_offsetY,     // 盤全体の原点シフト
        fieldRect,
        /*leftSide=*/false        // 右側の駒台へ配置
        );

    // (3) 段番号 → 後手の駒種（駒文字）へのマッピング
    QChar value = rankToWhiteShogiPiece(file, rank);

    // (4) 駒台セル内へアイコン描画（枚数0なら描かない／ドラッグ中は一時枚数を優先）
    //     ※ drawStandPieceIcon は painter の状態を汚さない前提。
    drawStandPieceIcon(painter, adjustedRect, value);
}

// 盤上および駒台（疑似座標 file=10/11）のハイライト矩形を塗る。
// 方針：盤上/駒台とも「駒アイコンと同じ算出経路」で矩形を作る。
//   1) 盤上 … calculateSquareRectangleBasedOnBoardState() の結果に m_offsetX/Y を足す
//   2) 駒台 … 疑似座標(file=10/11, rank=1..9) → 基準盤マス(file=1/2, rank=6..9 or 4..1)へ変換
//              → calculateSquareRectangleBasedOnBoardState() → makeStandCellRect() で駒台側へ水平シフト
// これにより、反転(m_flipMode)や左右の寄せ(m_param1/m_param2)、全体オフセット(m_offsetX/Y)が
// 駒描画と完全一致し、クリック位置とハイライト位置のズレが解消される。
void ShogiView::drawHighlights(QPainter* painter)
{
    if (!m_board) return;

    // 駒台の疑似座標 (10/11, 1..9) → 駒台描画で使う基準盤マス(file=1/2, rank=...) へ変換
    // 返り値：{ok, baseFile(=1/2), baseRank(黒:6..9 / 白:4..1), useBlackStand(true=先手駒台)}
    const auto standPseudoToBase = [&](int pseudoFile, int pseudoRank)
        -> std::tuple<bool,int,int,bool> {
        // 先手（黒）駒台：file==10
        if (pseudoFile == 10) {
            // 左列(=file=2)：R,G,N,P → rank 7,5,3,1 → 盤rank 6,7,8,9
            if (pseudoRank == 7 || pseudoRank == 5 || pseudoRank == 3 || pseudoRank == 1) {
                const int baseFile = 2;
                const int baseRank = 6 + ((7 - pseudoRank) / 2); // 7→6, 5→7, 3→8, 1→9
                return {true, baseFile, baseRank, true};
            }
            // 右列(=file=1)：K,B,S,L → rank 8,6,4,2 → 盤rank 6,7,8,9
            if (pseudoRank == 8 || pseudoRank == 6 || pseudoRank == 4 || pseudoRank == 2) {
                const int baseFile = 1;
                const int baseRank = 6 + ((8 - pseudoRank) / 2); // 8→6, 6→7, 4→8, 2→9
                return {true, baseFile, baseRank, true};
            }
            return {false, 0, 0, true};
        }

        // 後手（白）駒台：file==11
        if (pseudoFile == 11) {
            // 左列(=file=1)：r,g,n,p → rank 3,5,7,9 → 盤rank 4,3,2,1
            if (pseudoRank == 3 || pseudoRank == 5 || pseudoRank == 7 || pseudoRank == 9) {
                const int baseFile = 1;
                const int baseRank = (11 - pseudoRank) / 2;      // 3→4, 5→3, 7→2, 9→1
                return {true, baseFile, baseRank, false};
            }
            // 右列(=file=2)：k,b,s,l → rank 2,4,6,8 → 盤rank 4,3,2,1
            if (pseudoRank == 2 || pseudoRank == 4 || pseudoRank == 6 || pseudoRank == 8) {
                const int baseFile = 2;
                const int baseRank = (10 - pseudoRank) / 2;      // 2→4, 4→3, 6→2, 8→1
                return {true, baseFile, baseRank, false};
            }
            return {false, 0, 0, false};
        }

        // 盤上（疑似ではない）
        return {false, 0, 0, false};
    };

    // 盤/駒台共通：ハイライト矩形を算出
    const auto makeHighlightRect = [&](const FieldHighlight* fhl) -> QRect {
        const int f = fhl->file();
        const int r = fhl->rank();

        // 駒台の疑似座標（file=10/11）
        if (f == 10 || f == 11) {
            auto [ok, baseFile, baseRank, isBlackStand] = standPseudoToBase(f, r);
            if (!ok) return QRect(); // 変換不可はスキップ

            // 基準盤マス矩形 → 駒台側へ水平シフト
            const QRect base = calculateSquareRectangleBasedOnBoardState(baseFile, baseRank);
            const int   param = isBlackStand ? m_param1 : m_param2;
            const bool  leftSide = isBlackStand; // 先手駒台=左側, 後手駒台=右側
            const QRect adjusted = makeStandCellRect(m_flipMode, param, m_offsetX, m_offsetY,
                                                     base, leftSide);
            return adjusted;
        }

        // 通常の盤マス（file=1..9, rank=1..9）
        const QRect base = calculateSquareRectangleBasedOnBoardState(f, r);
        return QRect(base.left() + m_offsetX,
                     base.top()  + m_offsetY,
                     base.width(), base.height());
    };

    // 描画
    for (int i = 0; i < highlightCount(); ++i) {
        Highlight* hl = highlight(i);
        if (!hl || hl->type() != FieldHighlight::Type) continue;

        const auto* fhl = static_cast<FieldHighlight*>(hl);
        const QRect rect = makeHighlightRect(fhl);
        if (!rect.isNull()) {
            painter->fillRect(rect, fhl->color());
        }
    }
}

// クリック位置（ウィジェット座標）から、盤上のマス座標（file=x, rank=y）を求めるエントリポイント。
// 役割：
//  - 盤の反転状態（m_flipMode）に応じて、専用の変換関数へ委譲して座標を算出する。
//  - 戻り値は QPoint(file, rank)。盤外や無効の場合は実装側の規約に従い QPoint() などを返す想定。
// 注意：オフセット（m_offsetX/m_offsetY）やマスサイズ（m_squareSize）の考慮は
//       getClickedSquareIn*State() 側で行う。
QPoint ShogiView::getClickedSquare(const QPoint &clickPosition) const
{
    // 【反転時】上下左右が入れ替わるため、反転用の座標変換を使用
    if (m_flipMode) {
        return getClickedSquareInFlippedState(clickPosition);
    } else {
        // 【通常時】デフォルトの座標変換を使用
        return getClickedSquareInDefaultState(clickPosition);
    }
}

QPoint ShogiView::getClickedSquareInDefaultState(const QPoint& pos) const
{
    // 成駒を元の駒種に正規化（P/L/N/S/B/R/K/G の小文字を返す）
    auto baseFrom = [](QChar ch) -> QChar {
        const QChar l = ch.toLower();
        if      (l == 'q') return QChar('p'); // と金 -> 歩
        else if (l == 'm') return QChar('l'); // 成香 -> 香
        else if (l == 'o') return QChar('n'); // 成桂 -> 桂
        else if (l == 't') return QChar('s'); // 成銀 -> 銀
        else if (l == 'c') return QChar('b'); // 馬   -> 角
        else if (l == 'u') return QChar('r'); // 龍   -> 飛
        return l; // 金/玉/未成はそのまま
    };

    if (!m_board) return QPoint();

    const QSize fs = fieldSize().isValid() ? fieldSize()
                                           : QSize(m_squareSize, m_squareSize);
    const int w = fs.width();
    const int h = fs.height();

    // 盤の外形
    const QRect boardRect(m_offsetX, m_offsetY,
                          w * m_board->files(), h * m_board->ranks());

    // 1) 盤内クリック（デフォルト向き）
    if (boardRect.contains(pos)) {
        const int xIn = pos.x() - boardRect.left();
        const int yIn = pos.y() - boardRect.top();
        const int colFromLeft = xIn / w;             // 0..8
        const int rowFromTop  = yIn / h;             // 0..8
        const int file = m_board->files() - colFromLeft; // 9..1
        const int rank = rowFromTop + 1;                 // 1..9
        return QPoint(file, rank);
    }

    // 2) 駒台の外接矩形（正しい左右・上下に修正）
    //   先手（file=10）：右側 m_param2 基準、下帯 rank 5..8
    const QRect senteStandRect(m_offsetX + static_cast<int>(m_param2),
                               m_offsetY + 5 * h, 2 * w, 4 * h);
    //   後手（file=11）：左側 m_param1 基準、上帯 rank 0..3
    const QRect goteStandRect (m_offsetX - static_cast<int>(m_param1),
                              m_offsetY + 0 * h, 2 * w, 4 * h);

    // 3) 局面編集モード中のドラッグドロップ先（駒種でセル固定）
    if (m_positionEditMode && m_dragging) {
        if (senteStandRect.contains(pos)) {
            const QChar p = m_dragPiece;
            if (p != QChar(' ')) {
                const QChar up = baseFrom(p).toUpper(); // 成→未成→大文字
                if      (up == 'P') return QPoint(10, 1);
                else if (up == 'L') return QPoint(10, 2);
                else if (up == 'N') return QPoint(10, 3);
                else if (up == 'S') return QPoint(10, 4);
                else if (up == 'G') return QPoint(10, 5);
                else if (up == 'B') return QPoint(10, 6);
                else if (up == 'R') return QPoint(10, 7);
                else if (up == 'K') return QPoint(10, 8);
            }
            return QPoint();
        }
        if (goteStandRect.contains(pos)) {
            const QChar p = m_dragPiece;
            if (p != QChar(' ')) {
                const QChar low = baseFrom(p); // 成→未成（小文字）
                if      (low == 'p') return QPoint(11, 9);
                else if (low == 'l') return QPoint(11, 8);
                else if (low == 'n') return QPoint(11, 7);
                else if (low == 's') return QPoint(11, 6);
                else if (low == 'g') return QPoint(11, 5);
                else if (low == 'b') return QPoint(11, 4);
                else if (low == 'r') return QPoint(11, 3);
                else if (low == 'k') return QPoint(11, 2);
            }
            return QPoint();
        }
        // 盤外かつ駒台外
        return QPoint();
    }

    // 4) （1stクリック）従来の駒台セル判定はそのまま
    {
        // 先手（右側＝m_param2 側） rank 5..8
        float tempFile = (pos.x() - m_param2 - m_offsetX) / float(w);
        float tempRank = (pos.y() - m_offsetY) / float(h);
        int   file     = static_cast<int>(tempFile);
        int   rank     = static_cast<int>(tempRank);

        if (file == 0) {
            if (rank == 8) return QPoint(10, 1); // 歩 P
            if (rank == 7) return QPoint(10, 3); // 桂 N
            if (rank == 6) return QPoint(10, 5); // 金 G
            if (rank == 5) return QPoint(10, 7); // 飛 R
        } else if (file == 1) {
            if (rank == 8) return QPoint(10, 2); // 香 L
            if (rank == 7) return QPoint(10, 4); // 銀 S
            if (rank == 6) return QPoint(10, 6); // 角 B
            if (rank == 5) return QPoint(10, 8); // 玉 K
        }
    }
    {
        // 後手（左側＝m_param1 側） rank 0..3
        float tempFile = (pos.x() + m_param1 - m_offsetX) / float(w);
        float tempRank = (pos.y() - m_offsetY) / float(h);
        int   file     = static_cast<int>(tempFile);
        int   rank     = static_cast<int>(tempRank);

        if (file == 1) {
            if (rank == 0) return QPoint(11, 9); // 歩 p
            if (rank == 1) return QPoint(11, 7); // 桂 n
            if (rank == 2) return QPoint(11, 5); // 金 g
            if (rank == 3) return QPoint(11, 3); // 飛 r
        } else if (file == 0) {
            if (rank == 0) return QPoint(11, 8); // 香 l
            if (rank == 1) return QPoint(11, 6); // 銀 s
            if (rank == 2) return QPoint(11, 4); // 角 b
            if (rank == 3) return QPoint(11, 2); // 玉 k
        }
    }

    return QPoint();
}

QPoint ShogiView::getClickedSquareInFlippedState(const QPoint& pos) const
{
    auto baseFrom = [](QChar ch) -> QChar {
        const QChar l = ch.toLower();
        if      (l == 'q') return QChar('p');
        else if (l == 'm') return QChar('l');
        else if (l == 'o') return QChar('n');
        else if (l == 't') return QChar('s');
        else if (l == 'c') return QChar('b');
        else if (l == 'u') return QChar('r');
        return l;
    };

    if (!m_board) return QPoint();

    const QSize fs = fieldSize().isValid() ? fieldSize()
                                           : QSize(m_squareSize, m_squareSize);
    const int w = fs.width();
    const int h = fs.height();

    const QRect boardRect(m_offsetX, m_offsetY,
                          w * m_board->files(), h * m_board->ranks());

    // 1) 盤内クリック（反転：file 左→右、rank 上→下で逆）
    if (boardRect.contains(pos)) {
        const int xIn = pos.x() - boardRect.left();
        const int yIn = pos.y() - boardRect.top();
        const int colFromLeft = xIn / w;
        const int rowFromTop  = yIn / h;

        const int file = colFromLeft + 1;               // 1..9
        const int rank = m_board->ranks() - rowFromTop; // 9..1
        return QPoint(file, rank);
    }

    // 2) 駒台の外接矩形（デフォルト向きと同じ帯で判定）
    const QRect senteStandRect(m_offsetX + static_cast<int>(m_param2),
                               m_offsetY + 5 * h, 2 * w, 4 * h); // 先手（右・下帯）
    const QRect goteStandRect (m_offsetX - static_cast<int>(m_param1),
                              m_offsetY + 0 * h, 2 * w, 4 * h); // 後手（左・上帯）

    // 3) ドラッグ中の駒台ドロップ
    if (m_positionEditMode && m_dragging) {
        if (senteStandRect.contains(pos)) {
            const QChar p = m_dragPiece;
            if (p != QChar(' ')) {
                const QChar up = baseFrom(p).toUpper();
                if      (up == 'P') return QPoint(10, 1);
                else if (up == 'L') return QPoint(10, 2);
                else if (up == 'N') return QPoint(10, 3);
                else if (up == 'S') return QPoint(10, 4);
                else if (up == 'G') return QPoint(10, 5);
                else if (up == 'B') return QPoint(10, 6);
                else if (up == 'R') return QPoint(10, 7);
                else if (up == 'K') return QPoint(10, 8);
            }
            return QPoint();
        }
        if (goteStandRect.contains(pos)) {
            const QChar p = m_dragPiece;
            if (p != QChar(' ')) {
                const QChar low = baseFrom(p);
                if      (low == 'p') return QPoint(11, 9);
                else if (low == 'l') return QPoint(11, 8);
                else if (low == 'n') return QPoint(11, 7);
                else if (low == 's') return QPoint(11, 6);
                else if (low == 'g') return QPoint(11, 5);
                else if (low == 'b') return QPoint(11, 4);
                else if (low == 'r') return QPoint(11, 3);
                else if (low == 'k') return QPoint(11, 2);
            }
            return QPoint();
        }
        return QPoint();
    }

    // 4) （1stクリック）従来の駒台セル判定はそのまま
    {
        // ここは元の実装方針に合わせています（必要なら後で一体化可能）
        float tempFile = (pos.x() - m_param2 - m_offsetX) / float(w);
        float tempRank = (pos.y() - m_offsetY) / float(h);
        int   file     = static_cast<int>(tempFile);
        int   rank     = static_cast<int>(tempRank);

        if (file == 0) {
            if (rank == 8) return QPoint(10, 1);
            if (rank == 7) return QPoint(10, 3);
            if (rank == 6) return QPoint(10, 5);
            if (rank == 5) return QPoint(10, 7);
        } else if (file == 1) {
            if (rank == 8) return QPoint(10, 2);
            if (rank == 7) return QPoint(10, 4);
            if (rank == 6) return QPoint(10, 6);
            if (rank == 5) return QPoint(10, 8);
        }
    }
    {
        float tempFile = (pos.x() + m_param1 - m_offsetX) / float(w);
        float tempRank = (pos.y() - m_offsetY) / float(h);
        int   file     = static_cast<int>(tempFile);
        int   rank     = static_cast<int>(tempRank);

        if (file == 1) {
            if (rank == 0) return QPoint(11, 9);
            if (rank == 1) return QPoint(11, 7);
            if (rank == 2) return QPoint(11, 5);
            if (rank == 3) return QPoint(11, 3);
        } else if (file == 0) {
            if (rank == 0) return QPoint(11, 8);
            if (rank == 1) return QPoint(11, 6);
            if (rank == 2) return QPoint(11, 4);
            if (rank == 3) return QPoint(11, 2);
        }
    }

    return QPoint();
}

// マウスボタン解放時のイベント処理。
// 役割：
//  - 右ドラッグ中に右ボタンを離した場合は「ドラッグキャンセル」として処理し、rightClicked を通知
//  - 左ボタン解放：盤/駒台上のクリック位置を算出して clicked を通知（無効/エラー時は無視）
//  - 右ボタン解放（非ドラッグ時）：同様に rightClicked を通知（無効なら無視）
// ポイント：
//  - getClickedSquare() は盤上(1..9,1..9)に加え、駒台の疑似座標 (file=10/11) も返し得る
//  - endDrag() はドラッグ中の一時状態（カーソル追従や一時カウント等）を確実に解除するため、シグナル送出前に呼ぶ
void ShogiView::mouseReleaseEvent(QMouseEvent *event)
{
    // 【右ボタン解放 × ドラッグ中】→ ドラッグのキャンセル扱い
    if (m_dragging && event->button() == Qt::RightButton) {
        endDrag();                                      // 一時状態を解除（描画も含めて正しい静止状態へ）
        QPoint pt = getClickedSquare(event->pos());     // 右クリック位置（盤 or 駒台）
        emit rightClicked(pt);                          // キャンセル/コンテキスト用途として通知
        return;                                         // 以降の分岐に入らない
    }

    // 【左ボタン解放】
    if (event->button() == Qt::LeftButton) {
        QPoint pt = getClickedSquare(event->pos());     // クリック位置を盤/駒台のマス座標に変換

        //begin
        qDebug() << "----mouseReleaseEvent: pos=" << event->pos() << " -> square=" << pt;
        qDebug() << "pt.x()=" << pt.x() << ", pt.y()=" << pt.y();
        //end

        if (pt.isNull()) return;                        // 盤外等は無視
        if (m_errorOccurred) return;                    // 異常時はシグナル送出を抑止
        emit clicked(pt);                               // 左クリック通知（選択/移動確定など呼び出し側で処理）
    }
    // 【右ボタン解放（非ドラッグ時）】
    else if (event->button() == Qt::RightButton) {
        QPoint pt = getClickedSquare(event->pos());     // 右クリック位置（盤/駒台）
        if (pt.isNull()) return;                        // 盤外等は無視
        emit rightClicked(pt);                          // 右クリック通知（ハイライト解除/メニュー等は呼び出し側）
    }
}

// ハイライト（選択マス・指し手候補などの視覚効果）を追加する。
// 役割：コンテナに追加 → 再描画要求（非同期 update()）。
// 注意：ここでは所有権の移動/破棄は行わないため、new で生成した場合の delete 管理は呼び出し側の設計に依存。
//       QObject 継承やスマートポインタ（std::unique_ptr 等）での所有権管理を検討すると安全。
void ShogiView::addHighlight(ShogiView::Highlight* hl)
{
    m_highlights.append(hl); // 末尾に追加（同一要素の重複は許容）
    update();                // 非同期に再描画イベントをポスト（即時 repaint はしない）
}

// 指定のハイライトを 1 件だけ取り除く。
// 役割：removeOne は一致する最初の要素を削除（見つからなければ何もしない）→ 再描画要求。
// 注意：ポインタの remove であり delete は行わない。所有権の取り扱いに注意。
void ShogiView::removeHighlight(ShogiView::Highlight* hl)
{
    m_highlights.removeOne(hl); // 最初に一致した要素を除去
    update();                   // 変更を描画に反映
}

// すべてのハイライトを削除する（コンテナを空にする）。
// 役割：クリア → 再描画要求。
// 注意：clear() はポインタを捨てるだけで delete はしない。
//       ハイライトの所有権が本ビュー側にある設計なら、qDeleteAll(m_highlights); m_highlights.clear();
//       として実体破棄を行うことを検討（ダングリングポインタ/リーク対策）。
void ShogiView::removeHighlightAllData()
{
    m_highlights.clear(); // 全要素を除去（ポインタの所有権/破棄は別途の設計に依存）
    update();             // 反映のため再描画を要求
}

// 駒文字 → アイコン（QIcon）を一括で登録する初期化関数。
// 役割：Qt リソース（":/pieces/..."）から各駒の SVG を読み込み、
//       setPiece() で内部マップ（m_pieces）に関連付ける。
// 規約：大文字＝先手（Sente）、小文字＝後手（Gote）。
//       昇格駒は以下の対応（慣例的アルファベット）を使用：
//         Q=と金(と)、M=成香、O=成桂、T=成銀、C=馬（角成）、U=龍（飛成）
// 備考：setPiece() は update() を呼ぶが、同一イベントループ内では描画要求は合流されるため
//       パフォーマンス上の影響は通常軽微（必要ならバッチ登録に最適化可）。
void ShogiView::setPieces()
{
    // ── 先手（大文字） ──
    setPiece('P', QIcon(":/pieces/Sente_fu45.svg"));       // 歩
    setPiece('L', QIcon(":/pieces/Sente_kyou45.svg"));     // 香
    setPiece('N', QIcon(":/pieces/Sente_kei45.svg"));      // 桂
    setPiece('S', QIcon(":/pieces/Sente_gin45.svg"));      // 銀
    setPiece('G', QIcon(":/pieces/Sente_kin45.svg"));      // 金
    setPiece('B', QIcon(":/pieces/Sente_kaku45.svg"));     // 角
    setPiece('R', QIcon(":/pieces/Sente_hi45.svg"));       // 飛
    setPiece('K', QIcon(":/pieces/Sente_ou45.svg"));       // 王/玉（先手側）

    // 昇格駒（先手）
    setPiece('Q', QIcon(":/pieces/Sente_to45.svg"));       // と金（歩成）
    setPiece('M', QIcon(":/pieces/Sente_narikyou45.svg")); // 成香
    setPiece('O', QIcon(":/pieces/Sente_narikei45.svg"));  // 成桂
    setPiece('T', QIcon(":/pieces/Sente_narigin45.svg"));  // 成銀
    setPiece('C', QIcon(":/pieces/Sente_uma45.svg"));      // 馬（角成）
    setPiece('U', QIcon(":/pieces/Sente_ryuu45.svg"));     // 龍（飛成）

    // ── 後手（小文字） ──
    setPiece('p', QIcon(":/pieces/Gote_fu45.svg"));        // 歩
    setPiece('l', QIcon(":/pieces/Gote_kyou45.svg"));      // 香
    setPiece('n', QIcon(":/pieces/Gote_kei45.svg"));       // 桂
    setPiece('s', QIcon(":/pieces/Gote_gin45.svg"));       // 銀
    setPiece('g', QIcon(":/pieces/Gote_kin45.svg"));       // 金
    setPiece('b', QIcon(":/pieces/Gote_kaku45.svg"));      // 角
    setPiece('r', QIcon(":/pieces/Gote_hi45.svg"));        // 飛
    setPiece('k', QIcon(":/pieces/Gote_gyoku45.svg"));     // 玉（後手側）

    // 昇格駒（後手）
    setPiece('q', QIcon(":/pieces/Gote_to45.svg"));        // と金（歩成）
    setPiece('m', QIcon(":/pieces/Gote_narikyou45.svg"));  // 成香
    setPiece('o', QIcon(":/pieces/Gote_narikei45.svg"));   // 成桂
    setPiece('t', QIcon(":/pieces/Gote_narigin45.svg"));   // 成銀
    setPiece('c', QIcon(":/pieces/Gote_uma45.svg"));       // 馬（角成）
    setPiece('u', QIcon(":/pieces/Gote_ryuu45.svg"));      // 龍（飛成）
}

// 反転表示用の「駒文字 → アイコン」マッピングを一括登録する。
// 役割：盤の向きを反転した際に、先手/後手の見た目（面の向き）を入れ替えるため、
//       小文字（後手の駒文字）に先手用の画像を、大文字（先手の駒文字）に後手用の画像を割り当てる。
// 規約：通常は「大文字＝先手」「小文字＝後手」だが、flip 用では見た目の整合を保つために画像を逆転。
//       昇格駒の対応は setPieces() と同じ（Q=と金, M=成香, O=成桂, T=成銀, C=馬, U=龍）。
// 注意：setPiece() は内部で update() を呼ぶため多数回呼び出すと再描画要求が重なるが、
//       イベントループ内で合流されるため通常は問題ない（必要ならバッチ化の最適化を検討）。
void ShogiView::setPiecesFlip()
{
    // ── flip 時：小文字（元・後手）に先手の画像を割り当て ──
    setPiece('p', QIcon(":/pieces/Sente_fu45.svg"));        // 歩
    setPiece('l', QIcon(":/pieces/Sente_kyou45.svg"));      // 香
    setPiece('n', QIcon(":/pieces/Sente_kei45.svg"));       // 桂
    setPiece('s', QIcon(":/pieces/Sente_gin45.svg"));       // 銀
    setPiece('g', QIcon(":/pieces/Sente_kin45.svg"));       // 金
    setPiece('b', QIcon(":/pieces/Sente_kaku45.svg"));      // 角
    setPiece('r', QIcon(":/pieces/Sente_hi45.svg"));        // 飛
    setPiece('k', QIcon(":/pieces/Sente_gyoku45.svg"));     // 玉（先手側の意匠）

    // 昇格駒（flip：小文字→先手画像）
    setPiece('q', QIcon(":/pieces/Sente_to45.svg"));        // と金
    setPiece('m', QIcon(":/pieces/Sente_narikyou45.svg"));  // 成香
    setPiece('o', QIcon(":/pieces/Sente_narikei45.svg"));   // 成桂
    setPiece('t', QIcon(":/pieces/Sente_narigin45.svg"));   // 成銀
    setPiece('c', QIcon(":/pieces/Sente_uma45.svg"));       // 馬（角成）
    setPiece('u', QIcon(":/pieces/Sente_ryuu45.svg"));      // 龍（飛成）

    // ── flip 時：大文字（元・先手）に後手の画像を割り当て ──
    setPiece('P', QIcon(":/pieces/Gote_fu45.svg"));         // 歩
    setPiece('L', QIcon(":/pieces/Gote_kyou45.svg"));       // 香
    setPiece('N', QIcon(":/pieces/Gote_kei45.svg"));        // 桂
    setPiece('S', QIcon(":/pieces/Gote_gin45.svg"));        // 銀
    setPiece('G', QIcon(":/pieces/Gote_kin45.svg"));        // 金
    setPiece('B', QIcon(":/pieces/Gote_kaku45.svg"));       // 角
    setPiece('R', QIcon(":/pieces/Gote_hi45.svg"));         // 飛
    setPiece('K', QIcon(":/pieces/Gote_ou45.svg"));         // 王（後手側の意匠）

    // 昇格駒（flip：大文字→後手画像）
    setPiece('Q', QIcon(":/pieces/Gote_to45.svg"));         // と金
    setPiece('M', QIcon(":/pieces/Gote_narikyou45.svg"));   // 成香
    setPiece('O', QIcon(":/pieces/Gote_narikei45.svg"));    // 成桂
    setPiece('T', QIcon(":/pieces/Gote_narigin45.svg"));    // 成銀
    setPiece('C', QIcon(":/pieces/Gote_uma45.svg"));        // 馬（角成）
    setPiece('U', QIcon(":/pieces/Gote_ryuu45.svg"));       // 龍（飛成）
}

// マウス操作の入力モードを設定するセッター。
// 役割：クリックでの選択/移動（true）か、それ以外の操作方針（false）かを切り替えるためのフラグ。
// 注意：ここでは描画やシグナル送出は行わない。実際の挙動はマウスイベント処理側で参照される。
void ShogiView::setMouseClickMode(bool mouseClickMode)
{
    m_mouseClickMode = mouseClickMode;
}

// 1マスの基準サイズ（px）を返すゲッター。
// 役割：ドラッグ中の駒の描画サイズなど、単一のスカラー値が欲しい場面で使用。
// 備考：レイアウト計算には QSize を返す fieldSize() も存在する。
//       非正方表示等の可能性を考慮する場合は fieldSize() の利用も検討。
int ShogiView::squareSize() const
{
    return m_squareSize;
}

// 盤の表示スケールを 1px 分だけ拡大する。
// 手順：
//  1) m_squareSize をインクリメント（1マスの基準サイズを +1）
//  2) 依存しているレイアウトパラメータを再計算（recalcLayoutParams）
//  3) fieldSize を更新（setFieldSize）→ シグナル発行／updateGeometry／時計ラベル位置再計算（※内部で実施）
//  4) 念のため時計ラベルのジオメトリを再更新（※ setFieldSize 内で既に行っているため重複だが無害）
//  5) 再描画要求（update）
// 注意：極端に大きくし過ぎないように上限クリップを設けると安全（TODO）。
void ShogiView::enlargeBoard()
{
    m_squareSize++;                                    // (1) 1px 拡大
    recalcLayoutParams();                              // (2) レイアウト関連の再計算
    setFieldSize(QSize(m_squareSize, m_squareSize));   // (3) マスサイズ更新（シグナル/再レイアウト含む）

    // (4) 冗長だが安全側の再配置（setFieldSize 内でも呼ばれている想定）
    updateBlackClockLabelGeometry();
    updateWhiteClockLabelGeometry();

    update();                                          // (5) 再描画
}

// 盤の表示スケールを 1px 分だけ縮小する。
// 手順は enlargeBoard と同じだが、サイズをデクリメントする点のみ異なる。
// 注意：0 以下や小さ過ぎる値を避けるため、下限クリップ（例：最小 8〜16px 程度）を入れると安全（TODO）。
void ShogiView::reduceBoard()
{
    m_squareSize--;                                    // 1px 縮小
    recalcLayoutParams();                              // レイアウト関連の再計算
    setFieldSize(QSize(m_squareSize, m_squareSize));   // マスサイズ更新（シグナル/再レイアウト含む）

    // 冗長だが安全側の再配置
    updateBlackClockLabelGeometry();
    updateWhiteClockLabelGeometry();

    update();                                          // 再描画
}

// エラーフラグのセッター。
// 役割：内部状態 m_errorOccurred を更新し、描画処理やイベント処理側での早期リターン判定に用いる。
// 備考：この関数自体は再描画要求やシグナル送出を行わない。
//       必要に応じて、エラー遷移時にログ出力やユーザー通知を別途行う設計も検討。
void ShogiView::setErrorOccurred(bool newErrorOccurred)
{
    m_errorOccurred = newErrorOccurred;
}

// 局面編集モードのオン/オフを切り替えるセッター。
// 役割：
//  - モードの変化がなければ何もしない（無駄な再配置/再描画を避ける）
//  - モードに応じて、先手側の時計/名前ラベルの可視状態を切り替える（編集モード中は隠す）
//  - 時計ラベルのジオメトリ（位置・サイズ）を再計算してレイアウトに反映
// 注意：ここでは主に先手側ラベルの可視切り替えのみを行っている（白側は別箇所で制御している前提）。
void ShogiView::setPositionEditMode(bool positionEditMode)
{
    // 【無駄な処理の回避】状態に変化がない場合は早期 return
    if (m_positionEditMode == positionEditMode) return;

    // 【内部状態の更新】
    m_positionEditMode = positionEditMode;

    // 【UI可視性の切り替え】
    // 編集モードでは先手（黒）側の時計/名前ラベルを非表示にして編集領域を確保。
    if (m_blackClockLabel) m_blackClockLabel->setVisible(!m_positionEditMode);
    if (m_blackNameLabel)  m_blackNameLabel->setVisible(!m_positionEditMode);

    // 【ジオメトリ更新】
    // モード切り替えに伴うレイアウト変化を反映（フォント/サイズ比率が変わる設計にも対応）。
    updateBlackClockLabelGeometry();
    updateWhiteClockLabelGeometry();
}

// 駒台（スタンド）と盤の横方向ギャップを「何マスぶん（列数）」で設定するセッター。
// 役割：
//  - 入力 cols を [0.0, 2.0] の範囲にクランプ（qBound）して m_standGapCols に反映
//  - ギャップ変更に伴い、レイアウト関連パラメータを再計算（駒台の位置や各種オフセットが変化し得る）
//  - sizeHint などの変更を親レイアウトに通知（updateGeometry）
//  - 時計ラベルのジオメトリを再配置（ギャップに合わせた見栄えを保つ）
//  - 最後に再描画要求（update）
// 備考：範囲上限/下限はUIデザイン上の安全域。必要なら定数化して同一ポリシーで運用すると良い。
void ShogiView::setStandGapCols(double cols)
{
    // 【入力値のクランプ】負値や大きすぎる値を抑止（0.0〜2.0 の間に丸める）
    m_standGapCols = qBound(0.0, cols, 2.0);

    // 【レイアウト再計算】駒台オフセット等、内部の配置パラメータを最新化
    recalcLayoutParams();

    // 【レイアウト通知】sizeHint の変化を親レイアウトに伝える
    updateGeometry();

    // 【関連UIの再配置】時計ラベル等の位置/サイズを反映
    updateBlackClockLabelGeometry();
    updateWhiteClockLabelGeometry();

    // 【再描画要求】変更内容を画面に反映
    update();
}

void ShogiView::setFlipMode(bool newFlipMode)
{
    // 変化なしなら何もしない（無駄な再配置を避ける）
    if (m_flipMode == newFlipMode) return;

    // いま表示中の手番ラベルを記録しておく（反転後に可視状態を維持するため）
    QLabel* tlBlack = this->findChild<QLabel*>(QStringLiteral("turnLabelBlack"));
    QLabel* tlWhite = this->findChild<QLabel*>(QStringLiteral("turnLabelWhite"));
    const bool blackShown = (tlBlack && tlBlack->isVisible());
    const bool whiteShown = (tlWhite && tlWhite->isVisible());

    // 反転状態を更新
    m_flipMode = newFlipMode;

    // ▲/▼/▽/△ の付け替えなど、名前表示を反転状態に同期
    refreshNameLabels();

    // 先手/後手の時計・名前ラベルのジオメトリを反転後の座標系で確定
    updateBlackClockLabelGeometry();
    updateWhiteClockLabelGeometry();

    // ★ 手番ラベルも「反転後の名前/時計の最終ジオメトリ」に追従させる
    //    （フォント/スタイルのコピーと角丸無効化も relayout 内で同期されます）
    relayoutTurnLabels_();

    // 反転前の可視状態を復元（「次の手番」が見えなくなる問題を防ぐ）
    if (tlBlack) { tlBlack->setVisible(blackShown); tlBlack->raise(); }
    if (tlWhite) { tlWhite->setVisible(whiteShown); tlWhite->raise(); }

    // 再描画
    update();
}

// 反転モードの現在値を取得するゲッター。
// true=反転表示、false=通常表示。
bool ShogiView::getFlipMode() const
{
    return m_flipMode;
}

// 盤面と駒台（スタンド）を初期状態に戻すユーティリティ。
// 役割：
//  1) 既存のハイライト（選択・候補表示など）を全消去
//  2) 盤オブジェクトにゲーム初期化を指示（ShogiBoard::resetGameBoard）
//     ※ 駒配置や駒台の枚数リセット／均し（equalize）が必要なら、その具体処理は ShogiBoard 側の実装に委譲
//  3) 再描画要求（update）で UI を最新状態に反映
// 前提：board() が有効（nullptr でない）であること。
// 注意：必要に応じてエラーハンドリング（board() の空チェック）を追加してもよい。
void ShogiView::resetAndEqualizePiecesOnStands()
{
    removeHighlightAllData();     // (1) ハイライトを全消去

    board()->resetGameBoard();    // (2) 盤と駒台を初期化（具体的挙動は ShogiBoard 側の責務）

    update();                     // (3) 画面を再描画
}

// 平手初期局面（先手後手の持駒なし）に初期化する。
// 役割：
//  1) 既存ハイライトを全消去
//  2) SFEN 文字列（盤面/手番/持駒/手数）を ShogiBoard に適用
//  3) 駒台（持駒）を初期化（平手＝空）
//  4) 再描画要求
// SFEN 例：
//   "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"
//    └ 盤面9段を'/'区切りで上段から：小文字=後手, 大文字=先手, 数字=空マス連続数
//      " b " = 手番（b=先手, w=後手） / " - " = 持駒なし / " 1 " = 手数
void ShogiView::initializeToFlatStartingPosition()
{
    removeHighlightAllData();  // (1)

    QString flatInitialSFENStr =
        "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1";
    board()->setSfen(flatInitialSFENStr);  // (2)

    board()->initStand();  // (3) 平手なので駒台を空に
    update();              // (4)
}

// 詰将棋などの問題用初期局面に初期化する。
// 役割：
//  1) 既存ハイライトを全消去
//  2) 問題用の SFEN を適用（持駒あり）
//  3) 追加で先手の持駒 'K'（※実装依存：ここでは ShogiBoard の仕様に準拠）を1枚増やす
//  4) 再描画要求
// SFEN 例：
//  "5+r1kl/6p2/6Bpn/9/7P1/9/9/9/9 b RSb4g3s3n3l15p 1"
//   └ 盤面：'+'付きは成駒、" b " は先手番、持駒は連記（例：R,S,b,... と枚数）
// 注意：board() が nullptr でない前提。必要なら安全弁の null チェックを追加。
void ShogiView::shogiProblemInitialPosition()
{
    removeHighlightAllData();  // (1)

    QString shogiProblemInitialSFENStr =
        "5+r1kl/6p2/6Bpn/9/7P1/9/9/9/9 b RSb4g3s3n3l15p 1";
    board()->setSfen(shogiProblemInitialSFENStr);  // (2)

    board()->incrementPieceOnStand('K');  // (3) 先手持駒に K を1枚追加（仕様は ShogiBoard に依存）
    update();                              // (4)
}
// 盤面の先後を入れ替える（左右/上下の反転や先後の立場入替を ShogiBoard 側に依頼）。
// 役割：
//  1) 現在のハイライトを一旦すべて消去（反転後の座標と不整合になるのを防ぐ）
//  2) 盤オブジェクトに対して flipSides() を呼び、内部局面を反転
//     ※ 具体的な反転内容（駒配置の反転、持駒の入替、手番の扱い等）は ShogiBoard の実装に委譲
//  3) 再描画要求（update）で UI を最新状態に反映
// 前提：board() が有効（nullptr でない）であること。
void ShogiView::flipBoardSides()
{
    removeHighlightAllData();  // (1) ハイライトをクリア

    board()->flipSides();      // (2) 盤側で先後入替・反転処理を実施

    update();                  // (3) 画面再描画
}

QChar ShogiView::rankToBlackShogiPiece(const int file, const int rank) const
{
    // 右列(file=1): K, B, S, L
    if (file == 1) {
        switch (rank) {
        case 6: return 'K';
        case 7: return 'B';
        case 8: return 'S';
        case 9: return 'L';
        default: return ' '; // 想定外の段は空
        }
    }
    // 左列(file=2): R, G, N, P
    else if (file == 2) {
        switch (rank) {
        case 6: return 'R';
        case 7: return 'G';
        case 8: return 'N';
        case 9: return 'P';
        default: return ' '; // 想定外の段は空
        }
    }
    else {
        return ' '; // file が 1/2 以外なら空
    }
}

QChar ShogiView::rankToWhiteShogiPiece(const int file, const int rank) const
{
    // 左列(file=1): r, g, n, p
    if (file == 1) {
        switch (rank) {
        case 4: return 'r';
        case 3: return 'g';
        case 2: return 'n';
        case 1: return 'p';
        default: return ' '; // 想定外の段は空
        }
    }
    // 右列(file=2): k, b, s, l
    else if (file == 2) {
        switch (rank) {
        case 4: return 'k';
        case 3: return 'b';
        case 2: return 's';
        case 1: return 'l';
        default: return ' '; // 想定外の段は空
        }
    }
    else {
        return ' '; // file が 1/2 以外なら空
    }
}

// ドラッグ操作の開始処理。
// 役割：
//  - つまみ上げ元（from）の位置からドラッグ対象の駒文字を取得し、ドラッグ用の一時状態をセット
//  - 駒台（file=10/11）からのドラッグでは在庫（枚数）が 1 以上あることを確認してから開始
//  - 駒台表示更新のため、一時カウント（m_tempPieceStandCounts）を作成し、つまみ上げた分を減算
//  - 現在のマウス位置（ウィジェット座標）を記録して、paintEvent内の drawDraggingPiece で追従描画
void ShogiView::startDrag(const QPoint &from)
{
    // 【駒台からのドラッグ可否チェック】
    // file=10/11 は駒台。対象駒の在庫が 0 以下ならドラッグ開始しない。
    if ((from.x() == 10 || from.x() == 11)) {
        QChar piece = board()->getPieceCharacter(from.x(), from.y());
        if (board()->m_pieceStand.value(piece) <= 0) return;
    }

    // 【ドラッグ状態の確立】
    m_dragging  = true;                               // ドラッグ中フラグ
    m_dragFrom  = from;                               // つまみ上げ元（盤/駒台の座標）
    m_dragPiece = m_board->getPieceCharacter(from.x(), from.y()); // 対象駒
    m_dragPos   = mapFromGlobal(QCursor::pos());      // 現在のポインタ位置（ウィジェット座標）

    // 【駒台の一時枚数マップを作成】
    // 画面上はドラッグで 1 枚減った見え方にするため、つまみ上げた分をデクリメント。
    m_tempPieceStandCounts = m_board->m_pieceStand;
    if (from.x() == 10 || from.x() == 11) {
        m_dragFromStand = true;                       // 駒台からのドラッグ
        m_tempPieceStandCounts[m_dragPiece]--;        // 一時的に在庫を減らす
    } else {
        m_dragFromStand = false;                      // 盤上からのドラッグ
    }

    update(); // 再描画（ドラッグ中の駒・駒台枚数の見た目を反映）
}

// ドラッグ操作の終了/キャンセル処理。
// 役割：ドラッグ中フラグを落とし、一時枚数マップを破棄して表示を元に戻す。
// 備考：実際のドロップ適用（駒の移動/配置反映）は、呼び出し側のロジックで行う想定。
void ShogiView::endDrag()
{
    m_dragging = false;                // ドラッグ終了
    m_tempPieceStandCounts.clear();    // 一時的な駒台枚数をクリア（表示を通常状態へ）
    update();                          // 再描画
}

// マウス移動時のイベント処理。
// 役割：
//  - ドラッグ中であれば、現在位置を記録（m_dragPos）して再描画を要求（ドラッグ中の駒を追従表示）
//  - その後、基底クラスの mouseMoveEvent を呼んで他ハンドラ（ツールチップ等）に伝搬
// 注意：setMouseTracking(true) によりボタン未押下でも move が届く設計。
//       頻繁な update() はイベントループで合流されるが、必要なら将来的にスロットリングを検討。
void ShogiView::mouseMoveEvent(QMouseEvent* event)
{
    if (m_dragging) {
        m_dragPos = event->pos();  // 現在のポインタ位置（ウィジェット座標）を更新
        update();                  // 再描画（paintEvent→drawDraggingPiece で追従描画）
    }
    QWidget::mouseMoveEvent(event); // 既定処理・他のイベントフィルタへ伝搬
}

// 局面編集モードの現在状態を返すゲッター。
// 戻り値：true＝編集モード、false＝通常モード。
// 各描画/入力ハンドラはこのフラグを参照して挙動を切り替える。
bool ShogiView::positionEditMode() const
{
    return m_positionEditMode;
}

void ShogiView::resizeEvent(QResizeEvent* e)
{
    // 既定のレイアウト更新など
    QWidget::resizeEvent(e);

    // 既存のジオメトリ更新
    updateBlackClockLabelGeometry();
    updateWhiteClockLabelGeometry();

    // ★ 手番ラベルの再配置（名前/時計の確定ジオメトリに追従）
    relayoutTurnLabels_();
}

// 先手（黒）側の駒台全体を覆う境界矩形（バウンディングボックス）を算出して返す。
// 用途：ヒットテスト、再描画領域の最小化、ドラッグ中の当たり判定など。
// 仕様：
//  - 行数(rows)：編集モード=8 行（rank 2..9）、通常=7 行（rank 3..9）
//  - 基準セル：flip の有無とモードに応じて「左上セル」を (leftCol, topRank) で選ぶ
//  - X/Y：基準セルの左上に盤オフセット(m_offsetX/Y)を加算し、さらに横シフト(m_param1)を適用
//  - 幅/高さ：マス幅×2 列、マス高×rows 行
QRect ShogiView::blackStandBoundingRect() const
{
    if (!m_board) return {};

    // マスサイズ（無効時は m_squareSize で補完）
    const QSize fs = fieldSize().isValid() ? fieldSize()
                                           : QSize(m_squareSize, m_squareSize);

    // 行数はモードで変化
    const int rows     = 4;

    // 左上セル（基準セル）の rank / file を決定
    //  非反転：編集=rank 2, 通常=rank 6 / 反転：rank 9
    const int topRank  = m_flipMode ? 9 : 6;
    // 左列の column：反転=1、通常=2（駒台は2列ぶん想定）
    const int leftCol  = m_flipMode ? 1 : 2;

    // 基準セルの矩形（盤座標→ウィジェット座標は後で加算）
    const QRect cell = calculateSquareRectangleBasedOnBoardState(leftCol, topRank);

    // X は左右方向のシフト量 m_param1 の符号が flip で反転
    const int x = (m_flipMode ? (cell.left() - m_param1 + m_offsetX)
                              : (cell.left() + m_param1 + m_offsetX));
    const int y = cell.top() + m_offsetY;

    // 2 列 × rows 行
    const int w = fs.width() * 2;
    const int h = fs.height() * rows;

    return QRect(x, y, w, h);
}

// 後手（白）側の駒台全体を覆う境界矩形（バウンディングボックス）を算出して返す。
// 仕様・考え方は黒側と同様だが、基準 rank と横シフトに用いるパラメータが異なる（m_param2）。
QRect ShogiView::whiteStandBoundingRect() const
{
    if (!m_board) return {};

    // マスサイズ（無効時は m_squareSize で補完）
    const QSize fs = fieldSize().isValid() ? fieldSize()
                                           : QSize(m_squareSize, m_squareSize);

    // 行数はモードで変化
    const int rows    = 4;

    // 左上セル（基準セル）の rank / file を決定
    const int topRank = m_flipMode ? 4 : 1;
    const int leftCol = m_flipMode ? 1 : 2;

    // 基準セルの矩形
    const QRect cell = calculateSquareRectangleBasedOnBoardState(leftCol, topRank);

    // X は左右方向のシフト量 m_param2（flip で符号反転）
    const int x = (m_flipMode ? (cell.left() + m_param2 + m_offsetX)
                              : (cell.left() - m_param2 + m_offsetX));
    const int y = cell.top() + m_offsetY;

    // 2 列 × rows 行
    const int w = fs.width() * 2;
    const int h = fs.height() * rows;

    return QRect(x, y, w, h);
}

// 先手（黒）側：名前・時計ラベルの位置とサイズを更新する。
// 役割：
//  - 編集モード中はラベルを非表示にして早期 return
//  - 駒台のバウンディング矩形（blackStandBoundingRect）を基準に、
//    「上に置く → 下に置く → 重なっても駒台内に押し込む」の順で最適配置を決定
//  - ラベルのフォントサイズを矩形に合わせて調整（時計は fitLabelFontToRect、名前はスケール係数）
// ポイント：
//  - nameH/clockH はマス高さ fs.height() に比例（最小でも1マス分を確保）
//  - マージンは外側 marginOuter と名前・時計間 marginInner を使用
void ShogiView::updateBlackClockLabelGeometry()
{
    // ラベル未生成なら何もしない
    if (!m_blackClockLabel || !m_blackNameLabel) return;

    // 駒台の基準矩形が無効なら非表示
    const QRect stand = blackStandBoundingRect();
    if (!stand.isValid()) {
        m_blackClockLabel->hide();
        m_blackNameLabel->hide();
        return;
    }

    // マス寸法（無効時はフォールバック）
    const QSize fs = fieldSize().isValid() ? fieldSize()
                                           : QSize(m_squareSize, m_squareSize);

    // 配置用パラメータ
    const int marginOuter = 4;  // 駒台との外側マージン
    const int marginInner = 2;  // 名前と時計の間のマージン
    const int nameH  = qMax(int(fs.height() * 0.8), fs.height()); // 名前はやや小さめだが最低1マス分
    const int clockH = qMax(int(fs.height() * 0.9), fs.height()); // 時計はやや大きめだが最低1マス分

    // 左端は駒台に合わせる。Y は上側 or 下側 or 内側の順で決定。
    const int x = stand.left();
    const int yAbove = stand.top() - (nameH + marginInner + clockH) - marginOuter;  // 駒台の上
    const int yBelow = stand.bottom() + 1 + marginOuter;                            // 駒台の下

    QRect nameRect, clockRect;

    if (yAbove >= 0) {
        // 1) 盤の上端より上にはみ出さないなら上に配置
        nameRect  = QRect(x, yAbove, stand.width(), nameH);
        clockRect = QRect(x, nameRect.bottom() + 1 + marginInner, stand.width(), clockH);
    } else if (yBelow + nameH + marginInner + clockH <= height()) {
        // 2) 下側に十分なスペースがあれば下に配置
        nameRect  = QRect(x, yBelow, stand.width(), nameH);
        clockRect = QRect(x, nameRect.bottom() + 1 + marginInner, stand.width(), clockH);
    } else {
        // 3) どちらも無理なら、駒台の内側に縦に積んで配置し、下端オーバー分だけ上に押し上げる
        nameRect  = QRect(x, stand.top() + marginOuter, stand.width(), nameH);
        clockRect = QRect(x, nameRect.bottom() + 1 + marginInner, stand.width(), clockH);
        int overflow = (clockRect.bottom() + marginOuter) - stand.bottom();
        if (overflow > 0) {
            nameRect.translate(0, -overflow);
            clockRect.translate(0, -overflow);
        }
    }

    // ジオメトリ反映
    m_blackNameLabel->setGeometry(nameRect);
    m_blackClockLabel->setGeometry(clockRect);

    // フォント調整：時計は矩形にフィット、名前はスケール係数で調整
    fitLabelFontToRect(m_blackClockLabel, m_blackClockLabel->text(), clockRect, 2);
    QFont f = m_blackNameLabel->font();
    f.setPointSizeF(qMax(8.0, fs.height() * m_nameFontScale));
    m_blackNameLabel->setFont(f);

    // 前面に出して表示
    m_blackNameLabel->raise();
    m_blackClockLabel->raise();
    m_blackNameLabel->show();
    m_blackClockLabel->show();
}

// 後手（白）側：名前・時計ラベルの位置とサイズを更新する。
// 役割・手順は黒側と同一。基準矩形は whiteStandBoundingRect() を用いる。
// 備考：名前ラベルのフォントベースとして黒側のフォントを流用している（デザイン統一のため）。
void ShogiView::updateWhiteClockLabelGeometry()
{
    // ラベル未生成なら何もしない
    if (!m_whiteClockLabel || !m_whiteNameLabel) return;

    // 駒台の基準矩形が無効なら非表示
    const QRect stand = whiteStandBoundingRect();
    if (!stand.isValid()) {
        m_whiteClockLabel->hide();
        m_whiteNameLabel->hide();
        return;
    }

    // マス寸法（無効時はフォールバック）
    const QSize fs = fieldSize().isValid() ? fieldSize()
                                           : QSize(m_squareSize, m_squareSize);

    // 配置用パラメータ
    const int marginOuter = 4;
    const int marginInner = 2;
    const int nameH  = qMax(int(fs.height() * 0.8), fs.height());
    const int clockH = qMax(int(fs.height() * 0.9), fs.height());

    const int x      = stand.left();
    const int yAbove = stand.top() - (nameH + marginInner + clockH) - marginOuter;
    const int yBelow = stand.bottom() + 1 + marginOuter;

    QRect nameRect, clockRect;

    if (yAbove >= 0) {
        // 上に配置
        nameRect  = QRect(x, yAbove, stand.width(), nameH);
        clockRect = QRect(x, nameRect.bottom() + 1 + marginInner, stand.width(), clockH);
    } else if (yBelow + nameH + marginInner + clockH <= height()) {
        // 下に配置
        nameRect  = QRect(x, yBelow, stand.width(), nameH);
        clockRect = QRect(x, nameRect.bottom() + 1 + marginInner, stand.width(), clockH);
    } else {
        // 駒台内に押し込む（オーバー分だけ上へシフト）
        nameRect  = QRect(x, stand.top() + marginOuter, stand.width(), nameH);
        clockRect = QRect(x, nameRect.bottom() + 1 + marginInner, stand.width(), clockH);
        int overflow = (clockRect.bottom() + marginOuter) - stand.bottom();
        if (overflow > 0) {
            nameRect.translate(0, -overflow);
            clockRect.translate(0, -overflow);
        }
    }

    // ジオメトリ反映
    m_whiteNameLabel->setGeometry(nameRect);
    m_whiteClockLabel->setGeometry(clockRect);

    // フォント調整：時計はフィット、名前はスケール係数
    fitLabelFontToRect(m_whiteClockLabel, m_whiteClockLabel->text(), clockRect, 2);
    QFont f = m_blackNameLabel->font(); // 黒側の設定をベースに統一感を保つ
    f.setPointSizeF(qMax(8.0, fs.height() * m_nameFontScale));
    m_whiteNameLabel->setFont(f);

    // 前面に出して表示
    m_whiteNameLabel->raise();
    m_whiteClockLabel->raise();
    m_whiteNameLabel->show();
    m_whiteClockLabel->show();
}

// ラベルの表示矩形（rect）にテキスト（text）が収まるよう、フォントサイズを自動調整する。
// アルゴリズム：二分探索（lo..hi の範囲で 18 回反復）により、収まる最大ポイントサイズ ≒ lo を求める。
// 手順：
//  1) null チェック後、paddingPx 分だけ内側に縮めた評価用矩形 inner を作成
//  2) lo=6pt, hi=200pt を初期範囲として二分探索
//     - mid を仮サイズにセット → QFontMetrics で text の幅/高さを測定
//     - inner に収まれば lo=mid（さらに大きくできる）、収まらなければ hi=mid（小さくする）
//  3) 最終的に lo を採用して label->setFont()
// 備考：
//  - QFontMetrics::horizontalAdvance は文字列の描画幅をピクセルで返す（タブ/改行は想定外）。
//  - ラベル側の word-wrap や elide が有効な場合は実測と差異が出ることがあるため、必要に応じて調整。
//  - 18 回の反復で十分な精度（約 2^-18 の範囲）を得られる一方でコストも低い。
void ShogiView::fitLabelFontToRect(QLabel* label, const QString& text,
                                   const QRect& rect, int paddingPx)
{
    // (0) ラベルが無ければ何もしない
    if (!label) return;

    // (1) 内側にパディングを設けた評価矩形を作成
    const QRect inner = rect.adjusted(paddingPx, paddingPx, -paddingPx, -paddingPx);

    // (2) フォントサイズの探索範囲 [lo, hi]
    double lo = 6.0;
    double hi = 200.0;

    QFont f = label->font();

    // (3) 二分探索で「収まる最大サイズ」を求める
    for (int i = 0; i < 18; ++i) {
        const double mid = (lo + hi) * 0.5;
        f.setPointSizeF(mid);

        // 現在のフォントでテキスト寸法を取得
        QFontMetrics fm(f);
        const int w = fm.horizontalAdvance(text); // テキスト幅
        const int h = fm.height();                // テキスト高さ（ascent+descent）

        // inner 内に収まっていれば、さらに大きくできる余地がある
        if (w <= inner.width() && h <= inner.height()) {
            lo = mid;
        } else {
            hi = mid;
        }
    }

    // (4) 収まる最大サイズを適用
    f.setPointSizeF(lo);
    label->setFont(f);
}

// 先手（黒）側の時計表示テキストを設定する。
// 役割：
//  - ラベルの存在チェック
//  - テキストをセット
//  - 現在のラベル矩形（geometry）に収まるようフォントサイズを自動調整
void ShogiView::setBlackClockText(const QString& text)
{
    if (!m_blackClockLabel) return;
    m_blackClockLabel->setText(text);
    // ラベルの現在ジオメトリを基準に、内側 2px パディングでフィットさせる
    fitLabelFontToRect(m_blackClockLabel, text, m_blackClockLabel->geometry(), 2);
}

// 後手（白）側の時計表示テキストを設定する。
// 役割は黒側と同様。テキスト反映後に矩形へフォントをフィットさせる。
void ShogiView::setWhiteClockText(const QString& text)
{
    if (!m_whiteClockLabel) return;
    m_whiteClockLabel->setText(text);
    fitLabelFontToRect(m_whiteClockLabel, text, m_whiteClockLabel->geometry(), 2);
}

// 名前ラベル用のフォント倍率（スケール）を設定する。
// 役割：
//  - 入力スケールを [0.2, 1.0] にクランプ（極端な小さ過ぎ／大き過ぎを防止）
//  - 黒側ラベルのジオメトリ再計算をトリガし、フォントサイズ反映まで行う
// メモ：白側も同様に即時反映したい場合は updateWhiteClockLabelGeometry() の呼び出しを追加検討。
void ShogiView::setNameFontScale(double scale)
{
    m_nameFontScale = std::clamp(scale, 0.2, 1.0);
    updateBlackClockLabelGeometry();
}

// 文字列から特定のマーク（▲▼▽△）をすべて除去し、前後の空白をトリムして返す。
// 役割：名前ラベル等に付加される向き・状態表示用の三角記号を取り除いて、
//       純粋な表示名/テキストを得るヘルパ。
// 手順：
//  1) 入力文字列 s を作業用にコピー（t）
//  2) 対象マーク配列（U+25B2▲, U+25BC▼, U+25BD▽, U+25B3△）を順に remove（全出現を削除）
//  3) 最後に trimmed() で前後空白を除去し、返却
QString ShogiView::stripMarks(const QString& s)
{
    QString t = s;

    // 除去対象のマーク（黒/白の上下三角）
    static const QChar marks[] = {
        QChar(0x25B2), // ▲ BLACK UP-POINTING TRIANGLE
        QChar(0x25BC), // ▼ BLACK DOWN-POINTING TRIANGLE
        QChar(0x25BD), // ▽ WHITE DOWN-POINTING TRIANGLE
        QChar(0x25B3)  // △ WHITE UP-POINTING TRIANGLE
    };

    // すべての該当記号を削除
    for (QChar m : marks)
        t.remove(m);

    // 仕上げに前後の空白をトリム
    return t.trimmed();
}

// 先手（黒）側のプレイヤー名を設定する。
// 役割：
//  - 入力名から▲▼▽△などの装飾マークを除去（stripMarks）して基底文字列 m_blackNameBase に保持
//  - 表示用ラベル（向き/手番/反転などに依存）を refreshNameLabels() で更新
void ShogiView::setBlackPlayerName(const QString& name)
{
    m_blackNameBase = stripMarks(name);  // 装飾を除いた素の名前を保存
    refreshNameLabels();                 // ラベル表示を更新（向き/手番表示などは内部で決定）
}

// 後手（白）側のプレイヤー名を設定する。
// 役割は黒側と同様：装飾マークを除去して m_whiteNameBase に保存し、表示を更新。
void ShogiView::setWhitePlayerName(const QString& name)
{
    m_whiteNameBase = stripMarks(name);
    refreshNameLabels();
}

// プレイヤー名ラベル（先手/後手）の表示内容とツールチップを最新状態に更新する。
// 役割：
//  - 反転状態（m_flipMode）に応じて、名前の先頭に付ける向きマークを切り替える
//      * 非反転：先手=▲ / 後手=▽
//      * 反転　：先手=▼ / 後手=△
//  - ElideLabel に対して fullText を設定（省略表示時の元テキスト）
//  - ツールチップはプレーンな名前（装飾なし）を HTML エスケープし、簡易スタイルを付与して設定
// 注意：
//  - 本関数はラベルが生成済み（nullptr でない）場合のみ更新を行う
//  - stripMarks() 済みの素の名前（m_blackNameBase / m_whiteNameBase）を用いる
void ShogiView::refreshNameLabels()
{
    // ── 先手（黒）側 ──
    if (m_blackNameLabel) {
        // 向きマークを反転状態で切り替え
        const QString markBlack = m_flipMode ? QStringLiteral("▼") : QStringLiteral("▲");

        // 省略表示用ラベルに完全なテキスト（マーク + 素の名前）を設定
        m_blackNameLabel->setFullText(markBlack + m_blackNameBase);

        // ツールチップ生成用のローカルラムダ（HTML エスケープ＋簡易スタイル）
        auto mkTip = [](const QString& plain) {
            return QStringLiteral(
                       "<div style='background-color:#FFF9C4; color:#333;"
                       "border:1px solid #C49B00; padding:6px; white-space:nowrap;'>%1</div>")
                .arg(plain.toHtmlEscaped());
        };
        // 装飾のない素の名前をツールチップとして設定
        m_blackNameLabel->setToolTip(mkTip(m_blackNameBase));
    }

    // ── 後手（白）側 ──
    if (m_whiteNameLabel) {
        // 向きマークを反転状態で切り替え
        const QString markWhite = m_flipMode ? QStringLiteral("△") : QStringLiteral("▽");

        // 省略表示用ラベルに完全なテキスト（マーク + 素の名前）を設定
        m_whiteNameLabel->setFullText(markWhite + m_whiteNameBase);

        // 同じく簡易スタイルのツールチップ（プレーン名をエスケープして安全に表示）
        auto mkTip = [](const QString& plain) {
            return QStringLiteral(
                       "<div style='background-color:#FFF9C4; color:#333;"
                       "border:1px solid #C49B00; padding:6px; white-space:nowrap;'>%1</div>")
                .arg(plain.toHtmlEscaped());
        };
        m_whiteNameLabel->setToolTip(mkTip(m_whiteNameBase));
    }
}

// 盤・駒台・ラベルの相対配置に用いる内部パラメータを再計算する。
// 役割：
//  - マスサイズ（m_squareSize）とユーザー指定の駒台ギャップ（m_standGapCols）から、
//    ラベル帯や駒台の水平オフセット（m_param1/m_param2）、盤のXオフセット（m_offsetX）などを更新。
//  - 段/筋ラベルの描画に最低限必要な隙間（needGapPx）を満たすように、ユーザー指定よりも広い
//    実効ギャップ（effGapPx/effCols）を採用する（ラベルの食い込み防止）。
// 各値の意味：
//  - m_labelBandPx : 段・筋ラベル帯の高さ（px）
//  - m_labelFontPt : ラベル用フォントサイズ（pt）
//  - m_labelGapPx  : 盤とラベルのすき間（px）
//  - m_param1      : 先手側（黒）駒台列の水平基準オフセット（px）
//  - m_param2      : 後手側（白）駒台列の水平基準オフセット（px）
//  - m_offsetX     : 盤の左端X（boardLeftPx）に相当する水平オフセット（px）
//  - m_standGapPx  : 実効的な駒台ギャップ（px）
// メモ：tweak は微調整用の定数（将来的に 1px のズレ補正等を行いたい場合に使用）。
void ShogiView::recalcLayoutParams()
{
    // 【微調整係数】現状0。必要に応じて±1〜2px程度の補正に使う想定。
    constexpr int tweak = 0;

    // 【ラベル関連の基本サイズ】マスサイズに比例し、下限/上限を設けて視認性を担保
    m_labelBandPx = std::max(10, int(m_squareSize * 0.68));     // ラベル帯の高さ（px）
    m_labelFontPt = std::clamp(m_squareSize * 0.26, 5.0, 18.0); // ラベルフォントのポイントサイズ
    m_labelGapPx  = std::max(2,  int(m_squareSize * 0.12));     // 盤とラベルのすき間（px）

    // 【駒台の実効ギャップ算出】
    // ユーザー指定の列数（m_standGapCols：0.0〜2.0）を px に換算
    const int userGapPx = qRound(m_squareSize * m_standGapCols);

    // 段ラベルを描くのに最低限必要なギャップ（px）を確保
    const int needGapPx = minGapForRankLabelsPx();

    // 実効ギャップ＝ユーザー指定と必要量の大きい方
    const int effGapPx   = std::max(userGapPx, needGapPx);

    // 実効ギャップを「何マス分か（列数）」に換算（浮動小数）
    const double effCols = double(effGapPx) / double(m_squareSize);

    // 【水平方向の主要オフセットを計算】
    // 盤の左に 2 列、右に 9 列（標準の将棋盤の幅）を置き、その外側に実効ギャップ列（effCols）を付与するイメージ。
    // m_param1：先手（黒）側の駒台寄せ量（左側列の基準）
    m_param1     = qRound((2.0 + effCols) * m_squareSize) - tweak;

    // m_param2：後手（白）側の駒台寄せ量（右側列の基準）
    m_param2     = qRound((9.0 + effCols) * m_squareSize) - tweak;

    // 盤の左端X座標（boardLeftPx）＝先手側の寄せ量を基準に設定
    m_offsetX    = m_param1 + tweak;

    // 実効ギャップの px 値（後続の計算や描画に利用）
    m_standGapPx = qRound(effCols * m_squareSize);

    // ★ 手番ラベルも最新レイアウトに追従
    relayoutTurnLabels_();
}

// 指定段（rank）に対応する「段ラベル（漢数字）」を、盤の外縁と駒台の内縁のあいだに描画する。
// 方針：
//  - ペン色などの共通状態は上位（drawRanks）で設定済みを前提に、本関数ではフォントのみ調整。
//  - フォント設定による状態汚染を避けるため、局所的に save()/restore() を用いる（9回程度の呼び出しでコストは軽微）。
// 手順：
//  1) rank 行の基準セル矩形を取得し、y 座標とセル高さ h を決定
//  2) 反転状態に応じて、段ラベル帯を描く側（右側 or 左側）を選択
//  3) 盤の外縁（boardEdge）と駒台の内縁（innerEdge）の中点を xCenter とし、そこにラベル帯を配置
//  4) ラベル帯の幅 w は m_labelBandPx を基本に、利用可能な隙間（gapPx）内に収まるようクリップ
//  5) フォントサイズを m_labelFontPt × m_rankFontScale を基準に、セル高さの 90% を上限に調整
//  6) rank（1..9）に対応する漢数字を中央揃えで描画
void ShogiView::drawRank(QPainter* painter, const int rank) const
{
    if (!m_board) return;

    // (1) 指定段の基準セル（file=1）を取り、Y と高さを決める
    const QRect cell = calculateSquareRectangleBasedOnBoardState(1, rank);
    const int h = cell.height();
    const int y = cell.top() + m_offsetY;

    // (2) 反転していなければ右側に段ラベル、反転時は左側に段ラベル
    const bool rightSide = !m_flipMode;

    // 盤の外縁（右側 or 左側の外端）と、駒台の内縁（盤寄りの端）を取得
    const int boardEdge  = rightSide ? boardRightPx() : boardLeftPx();
    const int innerEdge  = standInnerEdgePx(rightSide);

    // (3) その中点を段ラベル帯の中心 X とする
    const int xCenter    = (boardEdge + innerEdge) / 2;

    // (4) ラベル帯の幅を決定（隙間が狭い場合はクリップ）
    int w = m_labelBandPx;
    const int gapPx = std::abs(innerEdge - boardEdge);
    if (w > gapPx - 2) w = std::max(12, gapPx - 2);

    // ラベル帯の描画矩形
    const QRect rankRect(xCenter - w/2, y, w, h);

    // (5) フォントを段ラベル用に調整（局所的に保存/復元）
    painter->save();
    QFont f = painter->font();
    double pt = m_labelFontPt * m_rankFontScale;
    pt = std::min(pt, h * 0.9);   // セル高さの 90% を上限に
    f.setPointSizeF(pt);
    painter->setFont(f);

    // (6) 1..9 段に対応する漢数字を中央揃えで描画
    static const QStringList rankTexts = { "一","二","三","四","五","六","七","八","九" };
    if (rank >= 1 && rank <= rankTexts.size()) {
        painter->drawText(rankRect, Qt::AlignCenter, rankTexts.at(rank - 1));
    }
    painter->restore();
}

// 指定筋（file）に対応する「筋ラベル（全角数字）」を、盤の上端または下端に描画する。
// 方針：
//  - ペン色などの共通状態は上位（drawFiles）で設定済み。本関数ではフォントのみを調整。
//  - フォント設定による状態汚染を避けるため、局所的に save()/restore() を用いる。
// 手順：
//  1) file 列の基準セル（rank=1）から X と幅 w を取得（盤オフセットを加味）
//  2) 反転状態に応じて、ラベル帯の配置先を決定
//      * 非反転：盤の「上側」空き領域に配置
//      * 反転　：盤の「下側」空き領域に配置
//     利用可能な空き高さ avail を求め、ラベル帯高さ h を m_labelBandPx を上限に調整
//  3) フォントサイズは m_labelFontPt を基準に、帯高さの 75% を上限に設定
//  4) file（1..9）に対応する全角数字を中央揃えで描画
void ShogiView::drawFile(QPainter* painter, const int file) const
{
    if (!m_board) return;

    // (1) 指定筋の基準セル（rank=1）から X と幅を算出（盤の左オフセットを反映）
    const QRect cell = calculateSquareRectangleBasedOnBoardState(file, 1);
    const int x = cell.left() + m_offsetX;
    const int w = cell.width();

    // ラベル帯の初期高さ（必要に応じて後でクリップ）
    int h = m_labelBandPx;
    int y = 0;

    // (2) 配置先の決定（反転時は下側、非反転時は上側）
    if (m_flipMode) {
        const int boardBottom = m_offsetY + m_squareSize * m_board->ranks(); // 盤の下端Y
        int avail = height() - boardBottom - 2;                              // 下側の空き
        if (avail <= 0) return;                                              // 余白なし
        h = std::min(h, std::max(8, avail - m_labelGapPx));                  // ギャップ分を差し引いて高さ確保
        y = boardBottom + m_labelGapPx;                                      // 盤下からギャップ分だけ離す
        if (y + h > height() - 1) y = height() - 1 - h;                      // 最下端を越えないよう補正
    } else {
        int avail = m_offsetY - 2;                                           // 上側の空き
        if (avail <= 0) return;
        h = std::min(h, std::max(8, avail - m_labelGapPx));                  // ギャップ分を差し引いて高さ確保
        y = m_offsetY - m_labelGapPx - h;                                    // 盤上からギャップ分だけ上へ
    }

    const QRect fileRect(x, y, w, h);

    // (3) フォント調整（局所保護）
    painter->save();
    QFont f = painter->font();
    f.setPointSizeF(std::min(m_labelFontPt, h * 0.75));  // 帯高さの 75% を上限に
    painter->setFont(f);

    // (4) 全角数字 １..９ を中央描画
    static const QStringList fileTexts = { "１","２","３","４","５","６","７","８","９" };
    if (file >= 1 && file <= fileTexts.size())
        painter->drawText(fileRect, Qt::AlignHCenter | Qt::AlignVCenter, fileTexts.at(file - 1));

    painter->restore();
}

// 段ラベル（漢数字）を盤の外縁と駒台の内縁の間に収めるために必要な
// 「最小の横方向ギャップ（px）」を見積もって返すヘルパ。
// 使途：recalcLayoutParams() でユーザー指定ギャップと比較し、不足分を補う。
// 手順：
//  1) ラベル帯の想定高さ bandH をマスサイズから算出（下限10px）
//  2) ラベル用フォントサイズを bandH を上限として設定し、QFontMetrics で文字幅を計測
//  3) 段ラベル候補（「一」〜「九」）の中で最大幅 wMax を求める
//  4) 左右パディング（padding×2）を加えて必要総幅を返す
int ShogiView::minGapForRankLabelsPx() const
{
    // (1) ラベル帯の高さ
    const int bandH = std::max(10, int(m_squareSize * 0.68));

    // (2) フォント準備：ポイントサイズは [5pt, bandH] にクリップ
    QFont f = this->font();
    f.setPointSizeF(std::clamp(m_labelFontPt, 5.0, double(bandH)));
    QFontMetrics fm(f);

    // (3) 漢数字 1..9 段の候補
    static const QStringList ranks = { "一","二","三","四","五","六","七","八","九" };

    // 最も幅の広い文字の幅を取得
    int wMax = 0;
    for (const QString& s : ranks)
        wMax = std::max(wMax, fm.horizontalAdvance(s));

    // (4) 左右パディング（最低 2px、通常は m_labelGapPx を採用）を加えた合計幅を返す
    const int padding = std::max(2, m_labelGapPx);
    return wMax + padding * 2;
}

// 駒台の「盤側の内縁」X座標を返すユーティリティ。
// 役割：段ラベルの配置計算などで、盤外の空き領域（駒台との間）の境界を知るために使用。
// 引数：rightSide=true  → 盤の右側の内縁（boardRightPx のさらに右にギャップ）
//       rightSide=false → 盤の左側の内縁（boardLeftPx のさらに左にギャップ）
// 備考：m_standGapPx は recalcLayoutParams() で算出される実効ギャップ幅（px）。
int ShogiView::standInnerEdgePx(bool rightSide) const
{
    const int gap = m_standGapPx;
    return rightSide ? (boardRightPx() + gap)  // 右側は盤の外側へ +gap
                     : (boardLeftPx()  - gap); // 左側は盤の外側へ -gap
}

// 段ラベル用フォントのスケールを設定するセッター。
// 役割：drawRank() 内で用いるフォントサイズを倍率で調整（見やすさの個人差に対応）。
// 処理：入力 scale を [0.5, 1.2] にクランプして m_rankFontScale に反映し、再描画を要求。
void ShogiView::setRankFontScale(double scale)
{
    m_rankFontScale = std::clamp(scale, 0.5, 1.2);
    update(); // スケール変更を即時反映
}

void ShogiView::applyTurnHighlight(bool blackIsActive)
{
    // 手番を更新
    m_blackActive = blackIsActive;

    // 手番切替直後はいったん「通常配色（Normal）」を適用
    // → 次の timeUpdated で applyClockUrgency() が Warn10/Warn5 に上書きします
    m_urgency = Urgency::Normal;

    // setUrgencyVisuals() 内で手番側を通常配色に、非手番側を font-weight=400 に統一
    setUrgencyVisuals(Urgency::Normal);
}

// 手番（アクティブサイド）を設定するセッター。
// 役割：内部フラグを更新し、名前/時計ラベルに手番ハイライトを反映。
void ShogiView::setActiveSide(bool blackTurn)
{
    m_blackActive = blackTurn;              // 先手が手番なら true
    applyTurnHighlight(m_blackActive);      // 背景/前景色を手番に合わせて適用
}

// 手番ハイライトの配色スタイルを設定する。
// 役割：背景色（ON 時）、文字色（ON/ OFF 時）を更新し、現手番に即時再適用。
// 注意：applyTurnHighlight はラベルが存在する場合にのみ安全に反映する。
void ShogiView::setHighlightStyle(const QColor& bgOn, const QColor& fgOn, const QColor& fgOff)
{
    m_highlightBg    = bgOn;    // アクティブ時の背景色
    m_highlightFgOn  = fgOn;    // アクティブ時の文字色
    m_highlightFgOff = fgOff;   // 非アクティブ時の文字色
    applyTurnHighlight(m_blackActive);  // 現在の手番にスタイルを再適用
}

// 盤（1 マス）のピクセルサイズ変更をレイアウトに反映するユーティリティ。
// 役割：fieldSize を m_squareSize の正方に設定し、レイアウトへ通知。
// 備考：setFieldSize() 側で updateGeometry() と時計ラベルジオメトリ更新を既に行っているため、
//       ここでの updateGeometry() は冗長だが無害（将来的に削除検討可）。
void ShogiView::updateBoardSize()
{
    setFieldSize(QSize(m_squareSize, m_squareSize)); // 1 マスのサイズを更新（内部で再レイアウトも行う）
    updateGeometry();                                // 冗長だが安全側の再通知

    // ★ 盤マスサイズ変更に伴い、手番ラベルの位置/フォントも追従
    relayoutTurnLabels_();
}

QString ShogiView::toRgb(const QColor& c)
{
    return QStringLiteral("rgb(%1,%2,%3)")
    .arg(QString::number(c.red()),
         QString::number(c.green()),
         QString::number(c.blue()));
}

void ShogiView::setLabelStyle(QLabel* lbl,
                              const QColor& fg, const QColor& bg,
                              int borderPx, const QColor& borderColor,
                              bool bold)
{
    if (!lbl) return;

    // ★ 角丸を 0px に
    const QString css = QStringLiteral(
                            "color:%1; background:%2; border:%3px solid %4; font-weight:%5; "
                            "padding:2px; border-radius:0px;")
                            .arg(
                                toRgb(fg),                         // %1
                                toRgb(bg),                         // %2
                                QString::number(borderPx),         // %3
                                toRgb(borderColor),                // %4
                                (bold ? QStringLiteral("700")
                                      : QStringLiteral("400"))     // %5
                                );

    lbl->setStyleSheet(css);
}

void ShogiView::setUrgencyVisuals(Urgency u)
{
    QLabel* actName    = m_blackActive ? m_blackNameLabel  : m_whiteNameLabel;
    QLabel* actClock   = m_blackActive ? m_blackClockLabel : m_whiteClockLabel;
    QLabel* inactName  = m_blackActive ? m_whiteNameLabel  : m_blackNameLabel;
    QLabel* inactClock = m_blackActive ? m_whiteClockLabel : m_blackClockLabel;

    // 非手番は font-weight=400 固定
    auto setInactive = [&](QLabel* name, QLabel* clock){
        const QColor inactiveFg(51, 51, 51);
        const QColor inactiveBg(239, 240, 241);
        setLabelStyle(name,  inactiveFg, inactiveBg, 0, QColor(0,0,0,0), /*bold=*/false);
        setLabelStyle(clock, inactiveFg, inactiveBg, 0, QColor(0,0,0,0), /*bold=*/false);
    };

    switch (u) {
    case Urgency::Normal:
        // 旧仕様：手番は黄背景＋青文字、枠なし、太字
        setLabelStyle(actName,  kTurnFg,   kTurnBg,   0, QColor(0,0,0,0), /*bold=*/true);
        setLabelStyle(actClock, kTurnFg,   kTurnBg,   0, QColor(0,0,0,0), /*bold=*/true);
        setInactive(inactName, inactClock);
        break;

    case Urgency::Warn10:
        // 旧仕様：10秒以下は黄色地＋枠無し＋太字
        // ※ kWarn10Border を青 (0,0,255) にしておくこと
        setLabelStyle(actName,  kWarn10Fg, kWarn10Bg, 0, kWarn10Border, /*bold=*/true);
        setLabelStyle(actClock, kWarn10Fg, kWarn10Bg, 0, kWarn10Border, /*bold=*/true);
        setInactive(inactName, inactClock);
        break;

    case Urgency::Warn5:
        // 旧仕様：5秒以下は黄色地＋枠無し＋太字
        // ※ kWarn5Border を赤 (255,0,0) にしておくこと
        setLabelStyle(actName,  kWarn5Fg,  kWarn5Bg,  0, kWarn5Border,  /*bold=*/true);
        setLabelStyle(actClock, kWarn5Fg,  kWarn5Bg,  0, kWarn5Border,  /*bold=*/true);
        setInactive(inactName, inactClock);
        break;
    }
}

void ShogiView::applyStartupTypography()
{
    m_urgency = Urgency::Normal;

    // 両側を「非手番スタイル（font-weight:400）」で統一
    const QColor inactiveFg(51, 51, 51);
    const QColor inactiveBg(239, 240, 241);
    auto setInactive = [&](QLabel* name, QLabel* clock){
        setLabelStyle(name,  inactiveFg, inactiveBg, /*borderPx=*/0, QColor(0,0,0,0), /*bold=*/false); // 400
        setLabelStyle(clock, inactiveFg, inactiveBg, /*borderPx=*/0, QColor(0,0,0,0), /*bold=*/false); // 400
    };

    setInactive(m_blackNameLabel,  m_blackClockLabel);
    setInactive(m_whiteNameLabel,  m_whiteClockLabel);
}

void ShogiView::setBlackTimeMs(qint64 ms) { m_blackTimeMs = ms; }
void ShogiView::setWhiteTimeMs(qint64 ms) { m_whiteTimeMs = ms; }

QImage ShogiView::toImage(qreal scale)
{
    QPixmap pm = this->grab(rect());        // 背景＋子ウィジェット込み
    QImage img = pm.toImage();
    if (!qFuzzyCompare(scale, 1.0)) {
        img = img.scaled(img.size() * scale,
                         Qt::IgnoreAspectRatio,
                         Qt::SmoothTransformation);
    }
    return img;
}

void ShogiView::applyBoardAndRender(ShogiBoard* board)
{
    if (!board) return;

    // 1) 駒アイコンの用意：現在の反転状態に合わせて読込
    if (m_flipMode) setPiecesFlip();
    else            setPieces();

    // 2) 盤データの適用
    setBoard(board);

    // 3) マスサイズの再適用（必要なら）
    setFieldSize(QSize(squareSize(), squareSize()));

    // 4) 再描画
    update();
}

void ShogiView::configureFixedSizing(int squarePx)
{
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    const int s = (squarePx > 0) ? squarePx : squareSize();
    setFieldSize(QSize(s, s));
    update();
}

void ShogiView::applyClockUrgency(qint64 activeRemainMs)
{
    Urgency next = Urgency::Normal;
    if      (activeRemainMs <= kWarn5Ms)  next = Urgency::Warn5;
    else if (activeRemainMs <= kWarn10Ms) next = Urgency::Warn10;

    if (next != m_urgency) {
        m_urgency = next;
        setUrgencyVisuals(m_urgency);
    }
}

void ShogiView::clearTurnHighlight()
{
    // 非手番配色（薄いグレー背景/通常フォント）を両陣営へ適用
    const QColor fg(51, 51, 51);
    const QColor bg(239, 240, 241);
    setLabelStyle(m_blackNameLabel,  fg, bg, 0, QColor(0,0,0,0), /*bold=*/false);
    setLabelStyle(m_blackClockLabel, fg, bg, 0, QColor(0,0,0,0), /*bold=*/false);
    setLabelStyle(m_whiteNameLabel,  fg, bg, 0, QColor(0,0,0,0), /*bold=*/false);
    setLabelStyle(m_whiteClockLabel, fg, bg, 0, QColor(0,0,0,0), /*bold=*/false);

    // 内部状態も「通常」に戻す（念のため）
    m_urgency = Urgency::Normal;
}

void ShogiView::setUiMuted(bool on) {
    m_uiMuted = on;
}

void ShogiView::setActiveIsBlack(bool activeIsBlack)
{
    m_blackActive = activeIsBlack;
}

// 手番ラベルを（無ければ）生成して初期設定する。
// ・参照元（黒=名前／白=時計）からフォント/色/背景をコピー
// ・角丸を無効化（border-radius:0）して四角い枠に統一
// ・内部マージンやラップを無効化して文字欠けを抑制
void ShogiView::ensureTurnLabels_()
{
    // 先手用
    QLabel* tlBlack = this->findChild<QLabel*>(QStringLiteral("turnLabelBlack"));
    if (!tlBlack) {
        tlBlack = new QLabel(tr("次の手番"), this);
        tlBlack->setObjectName(QStringLiteral("turnLabelBlack"));
        tlBlack->setAlignment(Qt::AlignCenter);
        tlBlack->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        tlBlack->setContentsMargins(0, 0, 0, 0);
        tlBlack->setMargin(0);
        tlBlack->setWordWrap(false);
        tlBlack->setTextFormat(Qt::PlainText);
        tlBlack->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        tlBlack->hide();
    }

    // 後手用
    QLabel* tlWhite = this->findChild<QLabel*>(QStringLiteral("turnLabelWhite"));
    if (!tlWhite) {
        tlWhite = new QLabel(tr("次の手番"), this);
        tlWhite->setObjectName(QStringLiteral("turnLabelWhite"));
        tlWhite->setAlignment(Qt::AlignCenter);
        tlWhite->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        tlWhite->setContentsMargins(0, 0, 0, 0);
        tlWhite->setMargin(0);
        tlWhite->setWordWrap(false);
        tlWhite->setTextFormat(Qt::PlainText);
        tlWhite->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        tlWhite->hide();
    }

    // 初期見た目をコピー + 角丸を外す
    if (m_blackNameLabel && tlBlack) {
        tlBlack->setFont(m_blackNameLabel->font());
        tlBlack->setStyleSheet(m_blackNameLabel->styleSheet());
        enforceSquareCorners(tlBlack);
    }
    if (m_whiteClockLabel && tlWhite) {
        tlWhite->setFont(m_whiteClockLabel->font());
        tlWhite->setStyleSheet(m_whiteClockLabel->styleSheet());
        enforceSquareCorners(tlWhite);
    }
}

void ShogiView::relayoutTurnLabels_()
{
    ensureTurnLabels_();

    QLabel* tlBlack = this->findChild<QLabel*>(QStringLiteral("turnLabelBlack"));
    QLabel* tlWhite = this->findChild<QLabel*>(QStringLiteral("turnLabelWhite"));

    QLabel* bn = m_blackNameLabel;  // 先手（黒）名前
    QLabel* bc = m_blackClockLabel; // 先手（黒）時計
    QLabel* wn = m_whiteNameLabel;  // 後手（白）名前
    QLabel* wc = m_whiteClockLabel; // 後手（白）時計
    if (!bn || !bc || !wn || !wc || !tlBlack || !tlWhite) return;

    // 角丸を完全無効化（見た目を統一）
    auto safeSquare = [](QLabel* lab){ if (lab) enforceSquareCorners(lab); };
    safeSquare(bn); safeSquare(bc); safeSquare(wn); safeSquare(wc);
    safeSquare(tlBlack); safeSquare(tlWhite);

    // 1マス寸法（fallbackあり）
    const QSize fs = fieldSize().isValid() ? fieldSize()
                                           : QSize(m_squareSize, m_squareSize);

    // ラベル高さの安全取得
    auto hLab = [&](QLabel* lab, int fallbackPx)->int {
        if (!lab) return fallbackPx;
        int h = lab->geometry().height();
        if (h <= 0) h = lab->sizeHint().height();
        if (h <= 0) h = fallbackPx;
        return h;
    };

    // 名前ラベルのフォントを（双方）同一スケールで調整
    {
        QFont f = bn->font();
        f.setPointSizeF(qMax(8.0, fs.height() * m_nameFontScale));
        bn->setFont(f);
        wn->setFont(f);
    }

    // 外側余白は維持、ラベル間の余白は 0 にする
    const int marginOuter = 2; // 駒台と最上段/最下段ラベルの外側余白
    const int marginInner = 0; // ★ ラベル間の余白をゼロに

    // 駒台外接矩形
    const QRect standBlack = blackStandBoundingRect();
    const QRect standWhite = whiteStandBoundingRect();
    if (!standBlack.isValid() || !standWhite.isValid()) return;

    const int BW = standBlack.width();
    const int BX = standBlack.left();
    const int WW = standWhite.width();
    const int WX = standWhite.left();

    // 各高さ
    const int HnBlack = hLab(bn, fs.height());
    const int HcBlack = hLab(bc, fs.height());
    const int HtBlack = hLab(tlBlack, fs.height());

    const int HnWhite = hLab(wn, fs.height());
    const int HcWhite = hLab(wc, fs.height());
    const int HtWhite = hLab(tlWhite, fs.height());

    // 可視制御は updateTurnIndicator() で行う
    tlBlack->hide();
    tlWhite->hide();

    // 直下に縦積み（駒台の下）: [lab1][lab2][lab3] を隙間ゼロで積む
    auto stackBelowStand = [&](const QRect& stand, int X, int W,
                               QLabel* lab1, int H1,
                               QLabel* lab2, int H2,
                               QLabel* lab3, int H3)
    {
        int y = stand.bottom() + 1 + marginOuter;
        QRect r1(X, y, W, H1);
        QRect r2(X, r1.bottom() + 1 + marginInner, W, H2); // marginInner=0 で密着
        QRect r3(X, r2.bottom() + 1 + marginInner, W, H3);

        // 画面下オーバー分は上に押し上げる（順序は保持）
        int overflow = (r3.bottom() + marginOuter) - height();
        if (overflow > 0) { r1.translate(0,-overflow); r2.translate(0,-overflow); r3.translate(0,-overflow); }

        lab1->setGeometry(r1);
        lab2->setGeometry(r2);
        lab3->setGeometry(r3);

        // 時計は枠にフィット
        if (lab2 == m_blackClockLabel || lab2 == m_whiteClockLabel) {
            fitLabelFontToRect(lab2, lab2->text(), r2, 2);
        }
    };

    // 直上に縦積み（駒台の上）: 上から [lab1][lab2][lab3] を隙間ゼロで積む
    auto stackAboveStand = [&](const QRect& stand, int X, int W,
                               QLabel* lab1, int H1,
                               QLabel* lab2, int H2,
                               QLabel* lab3, int H3)
    {
        const int totalH = H1 + marginInner + H2 + marginInner + H3; // marginInner=0
        int yTop = stand.top() - 1 - marginOuter - totalH;
        if (yTop < 0) yTop = 0;

        QRect r1(X, yTop, W, H1);
        QRect r2(X, r1.bottom() + 1 + marginInner, W, H2);
        QRect r3(X, r2.bottom() + 1 + marginInner, W, H3);

        lab1->setGeometry(r1);
        lab2->setGeometry(r2);
        lab3->setGeometry(r3);

        // 時計は枠にフィット
        if (lab3 == m_blackClockLabel || lab3 == m_whiteClockLabel) {
            fitLabelFontToRect(lab3, lab3->text(), r3, 2);
        }
    };

    // 並び順は以前の仕様通り（未反転: 左=後手・右=先手 / 反転: 左=先手・右=後手）
    if (!m_flipMode) {
        // 左（後手）：駒台の下に [名前][時計][次の手番]
        stackBelowStand(standWhite, WX, WW, wn, HnWhite, wc, HcWhite, tlWhite, HtWhite);
        // 右（先手）：駒台の上に [次の手番][名前][時計]
        stackAboveStand(standBlack, BX, BW, tlBlack, HtBlack, bn, HnBlack, bc, HcBlack);
    } else {
        // 左（先手）：駒台の下に [名前][時計][次の手番]
        stackBelowStand(standBlack, BX, BW, bn, HnBlack, bc, HcBlack, tlBlack, HtBlack);
        // 右（後手）：駒台の上に [次の手番][名前][時計]
        stackAboveStand(standWhite, WX, WW, tlWhite, HtWhite, wn, HnWhite, wc, HcWhite);
    }

    // Zオーダー
    bn->raise(); bc->raise(); wn->raise(); wc->raise();
    tlBlack->raise(); tlWhite->raise();
}

// 現在手番のみ「次の手番」を表示。
// ・表示直前に再配置（黒側は駒台直上アンカー、白側は従来通り）
// ・見た目は参照元に同期し、角丸は常に無効化
void ShogiView::updateTurnIndicator(ShogiGameController::Player now)
{
    if (!m_blackNameLabel || !m_whiteClockLabel) return;

    ensureTurnLabels_();

    QLabel* tlBlack = this->findChild<QLabel*>(QStringLiteral("turnLabelBlack"));
    QLabel* tlWhite = this->findChild<QLabel*>(QStringLiteral("turnLabelWhite"));
    if (!tlBlack || !tlWhite) return;

    tlBlack->hide();
    tlWhite->hide();

    // レイアウト更新（黒:駒台直上アンカー／白:従来）
    relayoutTurnLabels_();

    // 手番側のみ表示（見た目同期＆角丸オフは relayout 内で実施済）
    if (now == ShogiGameController::Player1) {
        tlBlack->show();
        tlBlack->raise();
    } else if (now == ShogiGameController::Player2) {
        tlWhite->show();
        tlWhite->raise();
    }
}
