#include "shogiview.h"
#include "shogiboard.h"
#include "enginesettingsconstants.h"
#include "elidelabel.h"
#include "solidtooltip.h"
#include <QColor>
#include <QMouseEvent>
#include <QPainter>
#include <QSettings>
#include <QDir>
#include <QApplication>

using namespace EngineSettingsConstants;

static SolidToolTip* g_tip = nullptr;

// コンストラクタ
ShogiView::ShogiView(QWidget *parent)
    : QWidget(parent),

    // エラーフラグをエラーが発生していない状態（false）で初期化する。
    m_errorOccurred(false),

    // 盤面の状態をノーマルモード（0）で初期化する。
    m_flipMode(0),

    // マウスクリックモードを有効（true）で初期化する。
    m_mouseClickMode(true),

    // 局面編集モードを無効（false）で初期化する。
    m_positionEditMode(false),

    m_dragging(false),

    // 将棋盤を表すShogiBoardオブジェクトのポインタをnullptrで初期化する。
    m_board(nullptr),

    // 駒台からつまんでない状態（false）で初期化する。
    m_dragFromStand(false)
{
    // カレントディレクトリをアプリケーションのディレクトリに設定する。
    QDir::setCurrent(QApplication::applicationDirPath());

    // 設定ファイルを読み込むためのQSettingsオブジェクトを初期化する。
    QSettings settings(SettingsFileName, QSettings::IniFormat);

    // マスのサイズを設定ファイルから読み込む。デフォルトは50。
    m_squareSize = settings.value("SizeRelated/squareSize", 50).toInt();

    // ★ここを直接代入ではなく再計算関数に
    recalcLayoutParams();

    setStandGapCols(0.5);

    // 盤を下にずらすオフセット
    m_offsetY = 20;

    setMouseTracking(true);

    m_blackClockLabel = new QLabel(QStringLiteral("00:00:00"), this);
    m_blackClockLabel->setObjectName(QStringLiteral("blackClockLabel"));
    m_blackClockLabel->setAlignment(Qt::AlignCenter);
    m_blackClockLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true); // マウスイベント透過
    m_blackClockLabel->setStyleSheet(QStringLiteral("background: transparent; color: black;"));

    {
        QFont f = font();
        f.setBold(true);
        f.setPointSizeF(qMax(8.0, m_squareSize * 0.45)); // マスサイズに連動
        m_blackClockLabel->setFont(f);
    }

    // ★ 対局者名（先手）：ElideLabel
    m_blackNameLabel = new ElideLabel(this);
    m_blackNameLabel->setElideMode(Qt::ElideRight);
    // ツールチップの付箋風スタイルは main.cpp の app.setStyleSheet でOK
    m_blackNameLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter); // 見やすいので左寄せ推奨
    m_blackNameLabel->setSlideOnHover(true);      // ホバーで自動スクロール
    m_blackNameLabel->setManualPanEnabled(true);  // 左ドラッグで手動パン
    m_blackNameLabel->setSlideSpeed(2);           // お好みで 1〜3
    m_blackNameLabel->setSlideInterval(16);       // 16ms=約60fps

    m_blackNameLabel->setContentsMargins(0, 10, 0, 0);
    m_blackClockLabel->setContentsMargins(0, 10, 0, 0);

    // ★ 対局者名（後手）：ElideLabel
    m_whiteNameLabel = new ElideLabel(this);
    m_whiteNameLabel->setElideMode(Qt::ElideRight);
    m_whiteNameLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_whiteNameLabel->setSlideOnHover(true);
    m_whiteNameLabel->setManualPanEnabled(true);
    m_whiteNameLabel->setSlideSpeed(2);
    m_whiteNameLabel->setSlideInterval(16);

    // 時計（後手）
    m_whiteClockLabel = new QLabel(QStringLiteral("00:00:00"), this);
    m_whiteClockLabel->setObjectName(QStringLiteral("whiteClockLabel"));
    m_whiteClockLabel->setAlignment(Qt::AlignCenter);
    m_whiteClockLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    m_whiteClockLabel->setStyleSheet(QStringLiteral("background: transparent; color: black;"));

    {
        QFont f = font();
        f.setBold(true);
        f.setPointSizeF(qMax(8.0, m_squareSize * 0.45));
        m_whiteClockLabel->setFont(f);
    }

    // お好みで余白
    m_whiteNameLabel->setContentsMargins(0, 10, 0, 0);
    m_whiteClockLabel->setContentsMargins(0, 10, 0, 0);

    // 位置決め
    updateBlackClockLabelGeometry();
    updateWhiteClockLabelGeometry();

    m_blackNameLabel->installEventFilter(this);
    m_whiteNameLabel->installEventFilter(this);
}

// m_boardにShogiBoardオブジェクトのポインタをセットする。
// 将棋盤データが更新された際にウィジェットを再描画するための設定も行う。
void ShogiView::setBoard(ShogiBoard* board)
{
    // 既に同じボードがセットされている場合は何もしない。
    if (m_board == board) return;

    if (m_board) {
        // 現在セットされているボードとのシグナル・スロット接続を解除する。
        // これにより、古いボードの変更がこのウィジェットに影響しないようにする。
        m_board->disconnect(this);
    }

    // 新しいボードをm_boardにセットする。
    m_board = board;

    if (board) {
        // 新しいボードのデータが変更された場合、このウィジェットを再描画するための接続を作成
        connect(board, &ShogiBoard::dataChanged, this, [this]{ update(); });
        // 新しいボードがリセットされた場合も、このウィジェットを再描画するための接続を作成
        connect(board, &ShogiBoard::boardReset, this, [this]{ update(); });
    }

    // ウィジェットのジオメトリ（サイズや形状）を更新する。
    updateGeometry();
}

// ...
bool ShogiView::eventFilter(QObject* obj, QEvent* ev)
{
    if (!g_tip) {
        g_tip = new SolidToolTip(this);
        g_tip->setCompact(true);      // お好みで
        g_tip->setPointSizeF(12.0);   // お好みで
    }

    if (obj == m_blackNameLabel || obj == m_whiteNameLabel) {
        if (ev->type() == QEvent::ToolTip) {
            auto* he = static_cast<QHelpEvent*>(ev);
            const QString text = (obj == m_blackNameLabel)
                                     ? m_blackNameBase : m_whiteNameBase;
            g_tip->showText(he->globalPos(), text);
            return true;              // 既定の QToolTip を抑止
        } else if (ev->type() == QEvent::Leave) {
            g_tip->hideTip();
        }
    }
    return QWidget::eventFilter(obj, ev);
}

// 現在セットされている将棋盤オブジェクトへのポインタを返す。
ShogiBoard *ShogiView::board() const
{
    return m_board;
}

// 現在のマスのサイズを返す。
QSize ShogiView::fieldSize() const
{
    return m_fieldSize;
}

// マスのサイズを設定する。
void ShogiView::setFieldSize(QSize fieldSize)
{
    // 既に同じサイズが設定されている場合は何もしない。
    if (m_fieldSize == fieldSize) return;

    m_fieldSize = fieldSize;

    // マスのサイズが変更されたことを通知するシグナルを発行する。
    emit fieldSizeChanged(fieldSize);

    // ウィジェットのジオメトリ（サイズや配置）を更新する。
    updateGeometry();

    updateBlackClockLabelGeometry();
    updateWhiteClockLabelGeometry();
}

// 将棋盤ウィジェットの推奨サイズを計算して返す。
QSize ShogiView::sizeHint() const
{
    // セーフティチェック：将棋盤オブジェクトが存在するか確認する。
    if (!m_board) {
        // 将棋盤オブジェクトが存在しない場合は、デフォルトサイズを返す。
        return QSize(100, 100);
    }

    // マスのサイズを取得する。
    const QSize squareSize = fieldSize();

    // 将棋盤の全列と全段を表示するのに必要なサイズを計算する。
    // 余白(m_offsetX, offsetY)を考慮して、将棋盤全体のサイズを決定する。
    int totalWidth = squareSize.width() * m_board->files() + m_offsetX * 2;

    int totalHeight = squareSize.height() * m_board->ranks() + m_offsetY * 2;

    // 将棋盤の表示に最適なサイズをQSizeオブジェクトとして返す。
    // このサイズは、レイアウトマネージャがウィジェットの配置を決定する際に使用される。
    return QSize(totalWidth, totalHeight);
}

// 盤面の状態（通常状態または反転状態）に応じて、指定された筋と段でのマスの矩形位置とサイズを計算する。
// この関数は、マス自体の描画や、マス上に駒を配置する際に使用される基本的な位置情報を提供する。
QRect ShogiView::calculateSquareRectangleBasedOnBoardState(const int file, const int rank) const
{
    // 盤面が存在しない場合は無効な矩形を返す。
    if (!m_board) return QRect();

    // マスのサイズを取得
    const QSize fs = fieldSize();

    // マスの画面上の位置を保持するための変数
    int xPosition, yPosition;

    // 盤面が反転している場合のX,Y座標を計算
    if (m_flipMode) {
        xPosition = (file - 1) * fs.width();
        yPosition = (m_board->ranks() - rank) * fs.height();
    }
    // 盤面が反転していない場合のX,Y座標を計算
    else {
        xPosition = (m_board->files() - file) * fs.width();
        yPosition = (rank - 1) * fs.height();
    }

    // 最終的なマスの矩形を作成して返す。
    return QRect(xPosition, yPosition, fs.width(), fs.height());
}

// 段番号や筋番号などのラベルを表示するための矩形を計算し、その表示位置を調整する。
// この関数は、主に盤面の外側に配置される番号ラベルのために使用され、表示位置に適切なオフセットを加えることで、
// 視覚的なレイアウトを整えるために利用する。
QRect ShogiView::calculateRectangleForRankOrFileLabel(const int file, const int rank) const
{
    // 盤面が存在しない場合は無効な矩形を返す。
    if (!m_board) return QRect();

    // マスのサイズを取得
    const QSize fs = fieldSize();

    // 調整後のX,Y座標を計算
    int adjustedX, adjustedY;

    adjustedX = (file - 1) * fs.width();

    // 盤面が反転している場合、座標を反転させる。
    if (m_flipMode) {
        adjustedY = (m_board->ranks() - rank) * fs.height();
    }
    // 盤面が反転していない場合のX,Y座標を計算
    else {
        adjustedY = (rank - 1) * fs.height();
    }

    // 最終的なマスの矩形を作成して返す。
    return QRect(adjustedX, adjustedY, fs.width(), fs.height()).translated(30, 0);
}

// 駒文字と駒画像をm_piecesに格納する。
// m_piecesの型はQMap<char, QIcon>
void ShogiView::setPiece(char type, const QIcon &icon)
{
    m_pieces.insert(type, icon);

    update();
}

// 指定された駒文字に対応するアイコンを返す。
// typeは、駒を表す文字。例: 'P'（歩）、'L'（香車）など
QIcon ShogiView::piece(QChar type) const
{
    return m_pieces.value(type, QIcon());
}

// 将棋盤に段番号を描画する。
void ShogiView::drawRanks(QPainter* painter)
{
    for (int r = 1; r <= m_board->ranks(); ++r) {
        painter->save();

        // 段番号rを描画する。
        drawRank(painter, r);

        painter->restore();
    }
}

// 将棋盤に筋番号を描画する。
void ShogiView::drawFiles(QPainter* painter)
{
    for (int c = 1; c <= m_board->files(); ++c) {
        painter->save();

        // 筋番号cを描画する。
        drawFile(painter, c);

        painter->restore();
    }
}

// 将棋盤の各マスを描画する。
void ShogiView::drawBoardFields(QPainter* painter)
{
    // 段
    for (int r = 1; r <= m_board->ranks(); ++r) {
        // 筋
        for (int c = 1; c <= m_board->files(); ++c) {
            painter->save();

            // マスを描画する。
            drawField(painter, c, r);

            painter->restore();
        }
    }
}

// 局面編集モードで先手駒台のマスを描画する。
void ShogiView::drawBlackEditModeStand(QPainter* painter)
{
    // 先手駒台
    // 段
    for (int r = 2; r <= 9; ++r) {
        // 列
        for (int c = 1; c <= 2; ++c) {
            painter->save();

            // 先手駒台のマスを描画する。
            drawBlackStandField(painter, c, r);

            painter->restore();
        }
    }
}

// 局面編集モードで後手駒台のマスを描画する。
void ShogiView::drawWhiteEditModeStand(QPainter* painter)
{
    // 後手駒台
    // 段
    for (int r = 1; r <= 8; ++r) {
        // 列
        for (int c = 1; c <= 2; ++c) {
            painter->save();

            // 後手駒台のマスを描画する。
            drawWhiteStandField(painter, c, r);

            painter->restore();
        }
    }
}

// 局面編集モードでは、先手と後手の駒台にある駒とその数を描画する。
void ShogiView::drawEditModeStand(QPainter* painter)
{
    // 局面編集モードで先手駒台のマスを描画する。
    drawBlackEditModeStand(painter);

    // 局面編集モードで後手駒台のマスを描画する。
    drawWhiteEditModeStand(painter);
}

// 通常モードで先手駒台のマスを描画する。
void ShogiView::drawBlackNormalModeStand(QPainter* painter)
{
    // 先手駒台
    // 段
    for (int r = 3; r <= 9; ++r) {
        // 列
        for (int c = 1; c <= 2; ++c) {
            painter->save();

            // 先手駒台のマスを描画する。
            drawBlackStandField(painter, c, r);

            painter->restore();
        }
    }
}

// 通常モードで後手駒台のマスを描画する。
void ShogiView::drawWhiteNormalModeStand(QPainter* painter)
{
    // 後手駒台
    // 段
    for (int r = 1; r <= 7; ++r) {
        // 列
        for (int c = 1; c <= 2; ++c) {
            painter->save();

            // 後手駒台のマスを描画する。
            drawWhiteStandField(painter, c, r);

            painter->restore();
        }
    }
}

// 通常モードでは、先手と後手の駒台を描画する。
void ShogiView::drawNormalModeStand(QPainter* painter)
{
    // 通常モードで先手駒台のマスを描画する。
    drawBlackNormalModeStand(painter);

    // 通常モードで後手駒台のマスを描画する。
    drawWhiteNormalModeStand(painter);
}

// 局面編集モードと通常モードに応じた駒台の描画処理を行う。
void ShogiView::drawEditModeFeatures(QPainter* painter)
{
    // 局面編集モードでは、先手と後手の駒台にある駒とその数を描画する。
    if (m_positionEditMode) {
        drawEditModeStand(painter);
    }
    // 通常モードでは、異なる方法で先手と後手の駒台を描画する。
    else {
        drawNormalModeStand(painter);
    }
}

// 将棋盤の駒を描画する。
void ShogiView::drawPieces(QPainter* painter)
{
    // 段
    for (int r = m_board->ranks(); r > 0; --r) {
        // 筋
        for (int c = 1; c <= m_board->files(); ++c) {
            painter->save();

            // 指定されたマスに駒を描画する。
            drawPiece(painter, c, r);

            // エラーが発生した場合は描画処理を中断する。
            if (m_errorOccurred) return;

            painter->restore();
        }
    }
}

// 局面編集モードで先手駒台に置かれた駒とその枚数を描画する。
void ShogiView::drawPiecesBlackStandInEditMode(QPainter* painter)
{
    // 先手駒台に置かれた駒とその枚数を描画する。
    for (int r = 2; r <= 9; ++r) {
        painter->save();

        // 先手駒台に置かれた駒を描画する。
        drawEditModeBlackStandPiece(painter, 2, r);

        // 先手駒台に置かれた駒の枚数を描画する。
        drawEditModeBlackStandPieceCount(painter, 1, r);

        painter->restore();
    }
}

// 局面編集モードで後手駒台に置かれた駒とその枚数を描画する。
void ShogiView::drawPiecesWhiteStandInEditMode(QPainter* painter)
{
    // 後手駒台に置かれた駒とその枚数を描画する。
    for (int r = 1; r <= 8; ++r) {
        painter->save();

        // 後手駒台に置かれた駒を描画する。
        drawEditModeWhiteStandPiece(painter, 1, r);

        // 後手駒台に置かれた駒の枚数を描画する。
        drawEditModeWhiteStandPieceCount(painter, 2, r);

        painter->restore();
    }
}

// 通常モードで先手駒台に置かれた駒とその枚数を描画する。
void ShogiView::drawPiecesBlackStandInNormalMode(QPainter* painter)
{
    // 先手駒台に置かれた駒とその枚数を描画する。
    for (int r = 3; r <= 9; ++r) {
        painter->save();

        // 先手駒台に置かれた駒を描画する。
        drawBlackStandPiece(painter, 2, r);

        // 先手駒台に置かれた駒の枚数を描画する。
        drawBlackStandPieceCount(painter, 1, r);

        painter->restore();
    }
}

// 通常モードで後手駒台に置かれた駒とその枚数を描画する。
void ShogiView::drawPiecesWhiteStandInNormalMode(QPainter* painter)
{
    // 後手駒台に置かれた駒とその枚数を描画する。
    for (int r = 1; r <= 7; ++r) {
        painter->save();

        // 後手駒台に置かれた駒を描画する。
        drawWhiteStandPiece(painter, 1, r);

        // 後手駒台に置かれた駒の枚数を描画する。
        drawWhiteStandPieceCount(painter, 2, r);

        painter->restore();
    }
}

// 駒台に置かれた駒とその数を描画する。
void ShogiView::drawPiecesEditModeStandFeatures(QPainter* painter)
{
    // 局面編集モードの場合
    if (m_positionEditMode) {
        // 先手駒台に置かれた駒とその枚数を描画する。
        drawPiecesBlackStandInEditMode(painter);

        // 後手駒台に置かれた駒とその枚数を描画する。
        drawPiecesWhiteStandInEditMode(painter);
    }
    // 通常モードの場合
    else {
        // 通常モードで先手駒台に置かれた駒とその枚数を描画する。
        drawPiecesBlackStandInNormalMode(painter);

        // 通常モードで後手駒台に置かれた駒とその枚数を描画する。
        drawPiecesWhiteStandInNormalMode(painter);
    }
}

// 将棋盤、駒台を描画する。
void ShogiView::paintEvent(QPaintEvent *)
{
    // 盤面が存在しない場合、あるいはエラーが発生した場合は何もしない。
    if (!m_board || m_errorOccurred) return;

    // QPainterオブジェクトを作成する。
    QPainter painter(this);

    // 将棋盤の各マスを描画する。
    drawBoardFields(&painter);

    // 局面編集モードと通常モードに応じて駒台の描画処理を行う。
    drawEditModeFeatures(&painter);

    // 将棋盤に4つの星を描画する。
    drawFourStars(&painter);

    // マスをハイライトする。
    drawHighlights(&painter);

    // 駒を描画する。
    drawPieces(&painter);

    // エラーが発生した場合は描画処理を中断する。
    if (m_errorOccurred) return;

    // 駒台に置かれた駒とその数を描画する。
    drawPiecesEditModeStandFeatures(&painter);

    // 将棋盤に段番号を描画する。
    drawRanks(&painter);

    // 将棋盤に筋番号を描画する。
    drawFiles(&painter);

    // ドラッグ中の駒を描画する。
    drawDraggingPiece();
}

// ドラッグ中の駒を描画する。
void ShogiView::drawDraggingPiece()
{
    // ドラッグ中の駒が存在しない場合
    if (m_dragging && m_dragPiece != ' ') {
        // 描画領域（マスと同サイズ）
        QRect r(m_dragPos.x() - squareSize()/2, m_dragPos.y() - squareSize()/2,
                squareSize(), squareSize());

        // ドラッグ中の駒を描画する。
        QPainter p(this);
        QIcon icon = piece(m_dragPiece);
        icon.paint(&p, r, Qt::AlignCenter);
    }
}

// 将棋盤に星（目印となる4つの点）を描画する。
void ShogiView::drawFourStars(QPainter *painter)
{
    // 星を描画するためのブラシを設定
    painter->setBrush(palette().color(QPalette::Dark));

    // 星の半径
    const int starRadius = 4;

    // 星を描画する位置を計算するための基準点
    const int basePoint3 = m_squareSize * 3;
    const int basePoint6 = m_squareSize * 6;

    // 4つの星（点）を盤面の特定の位置に描画する。
    painter->drawEllipse(QPoint(basePoint3 + m_offsetX, basePoint3 + m_offsetY), starRadius, starRadius);
    painter->drawEllipse(QPoint(basePoint6 + m_offsetX, basePoint3 + m_offsetY), starRadius, starRadius);
    painter->drawEllipse(QPoint(basePoint3 + m_offsetX, basePoint6 + m_offsetY), starRadius, starRadius);
    painter->drawEllipse(QPoint(basePoint6 + m_offsetX, basePoint6 + m_offsetY), starRadius, starRadius);
}

// shogiview.cpp
int ShogiView::boardLeftPx() const {
    return m_offsetX;
}
int ShogiView::boardRightPx() const {
    const int files = m_board ? m_board->files() : 9;
    return m_offsetX + m_squareSize * files;
}

// 盤面の指定されたマス（筋file、段rank）を描画する。
void ShogiView::drawField(QPainter* painter, const int file, const int rank) const
{
    // 盤面の指定されたマスの矩形を計算
    const QRect fieldRect = calculateSquareRectangleBasedOnBoardState(file, rank);

    // 描画するマスの位置を調整
    QRect adjustedRect = QRect(fieldRect.left() + m_offsetX, fieldRect.top() + m_offsetY, fieldRect.width(), fieldRect.height());

    // マスの塗りつぶし色を設定
    // 盤面のマスの色
    QColor fillColor(222, 196, 99, 255);

    // マスの境界線の色
    painter->setPen(QColor(30,30,30));

    // マスの塗りつぶし色を適用
    painter->setBrush(fillColor);

    // 調整された位置にマスを描画
    painter->drawRect(adjustedRect);
}

// 先手駒台のマスを描画する。
void ShogiView::drawBlackStandField(QPainter* painter, const int file, const int rank) const
{
    // 駒台のマスの位置を計算
    const QRect fieldRect = calculateSquareRectangleBasedOnBoardState(file, rank);

    // 描画位置を調整
    QRect adjustedRect;

    if (m_flipMode) {
        // 盤面が反転している場合、位置を左側に調整
        adjustedRect.setRect(fieldRect.left() - m_param1 + m_offsetX, fieldRect.top() + m_offsetY, fieldRect.width(), fieldRect.height());
    } else {
        // 盤面が反転していない場合、位置を右側に調整
        adjustedRect.setRect(fieldRect.left() + m_param1 + m_offsetX, fieldRect.top() + m_offsetY, fieldRect.width(), fieldRect.height());
    }

    // 駒台のマスの塗りつぶし色を設定
    QColor fillColor(228, 167, 46, 255);

    // 境界線の色も同じに設定
    painter->setPen(fillColor);

    // 塗りつぶし色を適用
    painter->setBrush(fillColor);

    // 調整された位置に駒台のマスを描画
    painter->drawRect(adjustedRect);
}

// 後手駒台のマスを描画する。
void ShogiView::drawWhiteStandField(QPainter* painter, const int file, const int rank) const
{
    // 駒台のマスの位置を計算
    const QRect fieldRect = calculateSquareRectangleBasedOnBoardState(file, rank);

    // 描画位置を調整
    QRect adjustedRect;

    if (m_flipMode) {
        // 盤面が反転している場合、位置を右側に調整
        adjustedRect.setRect(fieldRect.left() + m_param2 + m_offsetX, fieldRect.top() + m_offsetY, fieldRect.width(), fieldRect.height());
    } else {
        // 盤面が反転していない場合、位置を左側に調整
        adjustedRect.setRect(fieldRect.left() - m_param2 + m_offsetX, fieldRect.top() + m_offsetY, fieldRect.width(), fieldRect.height());
    }

    // 駒台のマスの塗りつぶし色を設定
    QColor fillColor(228, 167, 46, 255);

    // 境界線の色も同じに設定
    painter->setPen(fillColor);

    // 塗りつぶし色を適用
    painter->setBrush(fillColor);

    // 調整された位置に駒台のマスを描画
    painter->drawRect(adjustedRect);
}

// 盤面の指定されたマス（筋file、段rank）に駒を描画する。
void ShogiView::drawPiece(QPainter* painter, const int file, const int rank)
{
    if (m_dragging
        && file == m_dragFrom.x()
        && rank == m_dragFrom.y()) {
        // ドラッグ中はこのマスを描かない
        return;
    }

    // 盤面の指定されたマスの矩形を計算
    const QRect fieldRect = calculateSquareRectangleBasedOnBoardState(file, rank);

    // 描画する駒の位置を調整
    QRect adjustedRect = QRect(fieldRect.left() + m_offsetX, fieldRect.top() + m_offsetY, fieldRect.width(), fieldRect.height());

    // 盤面から駒の種類を取得
    QChar value;

    try {
        value = m_board->getPieceCharacter(file, rank);
    } catch (const std::exception& e) {
        // エラーフラグをエラー発生状態に設定する。
        m_errorOccurred = true;

        // エラーメッセージを通知する。
        QString errorMessage = QString(e.what());

        // エラーが発生したことをシグナルで通知する。
        emit errorOccurred(errorMessage);

        // エラーが発生した場合は描画処理を中断する。
        return;
    }

    // 駒が存在する場合、その駒のアイコンを描画する。
    if (value != ' ') {
        QIcon icon = piece(value);

        if (!icon.isNull()) {
            icon.paint(painter, adjustedRect, Qt::AlignCenter);
        }
    }
}

// 駒台の駒画像を描画する。
void ShogiView::drawStandPieceIcon(QPainter* painter, const QRect& adjustedRect, QChar value) const
{
    // ドラッグ中かつ一時マップにあるならそちらを優先する。
    int count = (m_dragging && m_tempPieceStandCounts.contains(value))
                    ? m_tempPieceStandCounts[value]
                    : m_board->m_pieceStand.value(value);

    if (count > 0 && value != ' ') {
        QIcon icon = piece(value);
        if (!icon.isNull()) {
            icon.paint(painter, adjustedRect, Qt::AlignCenter);
        }
    }
}

// 先手駒台に置かれた駒を描画する。
void ShogiView::drawBlackStandPiece(QPainter* painter, const int file, const int rank) const
{
    // 駒台のマスの位置を計算
    const QRect fieldRect = calculateSquareRectangleBasedOnBoardState(file, rank);

    // 描画位置を調整
    QRect adjustedRect;

    if (m_flipMode) {
        // 盤面が反転している場合、位置を左側に調整
        adjustedRect.setRect(fieldRect.left() - m_param1 + m_offsetX, fieldRect.top() + m_offsetY, fieldRect.width(), fieldRect.height());
    } else {
        // 盤面が反転していない場合、位置を右側に調整
        adjustedRect.setRect(fieldRect.left() + m_param1 + m_offsetX, fieldRect.top() + m_offsetY, fieldRect.width(), fieldRect.height());
    }

    // 駒台にある駒の種類を取得
    QChar value = rankToBlackShogiPiece(rank);

    // 駒台の駒画像を描画する。
    drawStandPieceIcon(painter, adjustedRect, value);
}

// 駒台の持ち駒の枚数を描画する。
void ShogiView::drawStandPieceCount(QPainter* painter, const QRect& adjustedRect, QChar value) const
{
    int count = (m_dragging && m_tempPieceStandCounts.contains(value))
    ? m_tempPieceStandCounts[value]
    : m_board->m_pieceStand.value(value);

    QString pieceCountText;

    // 局面編集モードの場合
    if (m_positionEditMode) {
        pieceCountText = QString::number(count);
    }
    // 通常モードの場合
    else {
        pieceCountText = (count > 0) ? QString::number(count) : QStringLiteral(" ");
    }

    painter->drawText(adjustedRect, Qt::AlignVCenter | Qt::AlignCenter, pieceCountText);
}

ElideLabel *ShogiView::blackNameLabel() const
{
    return m_blackNameLabel;
}

QLabel *ShogiView::blackClockLabel() const
{
    return m_blackClockLabel;
}

ElideLabel *ShogiView::whiteNameLabel() const
{
    return m_whiteNameLabel;
}

// 先手駒台に置かれた駒の枚数を描画する。
void ShogiView::drawBlackStandPieceCount(QPainter* painter, const int file, const int rank) const
{
    // 駒台のマスの位置を計算
    const QRect fieldRect = calculateSquareRectangleBasedOnBoardState(file, rank);

    // 描画位置を調整
    QRect adjustedRect;

    if (m_flipMode) {
        // 盤面が反転している場合、位置を左側に調整
        adjustedRect.setRect(fieldRect.left() - m_param1 + m_offsetX, fieldRect.top() + m_offsetY, fieldRect.width(), fieldRect.height());
    } else {
        // 盤面が反転していない場合、位置を右側に調整
        adjustedRect.setRect(fieldRect.left() + m_param1 + m_offsetX, fieldRect.top() + m_offsetY, fieldRect.width(), fieldRect.height());
    }

    // 駒台にある駒の種類に応じて枚数を取得し、描画するテキストを設定する。
    QChar value = rankToBlackShogiPiece(rank);

    // 駒台の持ち駒の枚数を描画する。
    drawStandPieceCount(painter, adjustedRect, value);
}

// 後手駒台に置かれた駒を描画する。
void ShogiView::drawWhiteStandPiece(QPainter* painter, const int file, const int rank) const
{
    // 駒台のマスの位置を計算
    const QRect fieldRect = calculateSquareRectangleBasedOnBoardState(file, rank);

    // 描画位置を調整
    QRect adjustedRect;

    if (m_flipMode) {
        // 盤面が反転している場合、位置を右側に調整
        adjustedRect.setRect(fieldRect.left() + m_param2 + m_offsetX, fieldRect.top() + m_offsetY, fieldRect.width(), fieldRect.height());
    } else {
        // 盤面が反転していない場合、位置を左側に調整
        adjustedRect.setRect(fieldRect.left() - m_param2 + m_offsetX, fieldRect.top() + m_offsetY, fieldRect.width(), fieldRect.height());
    }

    // 駒台にある駒の種類を取得し、駒が存在する場合そのアイコンを描画
    QChar value = rankToWhiteShogiPiece(rank);

    // 駒台の駒画像を描画する。
    drawStandPieceIcon(painter, adjustedRect, value);
}

// 後手駒台に置かれた駒の枚数を描画する。
void ShogiView::drawWhiteStandPieceCount(QPainter* painter, const int file, const int rank) const
{
    // 駒台のマスの位置を計算
    const QRect fieldRect = calculateSquareRectangleBasedOnBoardState(file, rank);

    // 描画位置を調整
    QRect adjustedRect;

    if (m_flipMode) {
        // 盤面が反転している場合、位置を右側に調整
        adjustedRect.setRect(fieldRect.left() + m_param2 + m_offsetX, fieldRect.top() + m_offsetY, fieldRect.width(), fieldRect.height());
    } else {
        // 盤面が反転していない場合、位置を左側に調整
        adjustedRect.setRect(fieldRect.left() - m_param2 + m_offsetX, fieldRect.top() + m_offsetY, fieldRect.width(), fieldRect.height());
    }

    // 駒台にある駒の種類に応じて枚数を取得し、描画するテキストを設定
    //QString pieceCountText;

    QChar value = rankToWhiteShogiPiece(rank);

    // 駒台の持ち駒の枚数を描画する。
    drawStandPieceCount(painter, adjustedRect, value);
}

// マスをハイライトする。
void ShogiView::drawHighlights(QPainter *painter)
{
    if (m_flipMode) {
        for (int idx = 0; idx < highlightCount(); ++idx) {
            Highlight *hl = highlight(idx);
            if (hl->type() == FieldHighlight::Type) {
                FieldHighlight *fhl = static_cast<FieldHighlight *>(hl);
                if (fhl->file() == 10) {
                    QRect r = calculateSquareRectangleBasedOnBoardState(fhl->file(), 10 - fhl->rank());
                    QRect rect = QRect(r.left() - m_param2 - m_squareSize + m_offsetX, r.top() + m_offsetY, r.width(), r.height());
                    painter->fillRect(rect, fhl->color());
                } else if (fhl->file() == 11) {
                    QRect r = calculateSquareRectangleBasedOnBoardState(fhl->file(), fhl->rank());
                    QRect rect = QRect(r.left() - m_squareSize * 10 + m_param2 + m_offsetX, m_squareSize * 8 - r.top() + m_offsetY, r.width(), r.height());
                    painter->fillRect(rect, fhl->color());
                } else {
                    QRect r = calculateSquareRectangleBasedOnBoardState(fhl->file(), fhl->rank());
                    QRect rect = QRect(r.left() + m_offsetX, r.top() + m_offsetY, r.width(), r.height());
                    painter->fillRect(rect, fhl->color());
                }
            }
        }
    } else {
        for (int idx = 0; idx < highlightCount(); ++idx) {
            Highlight *hl = highlight(idx);
            if (hl->type() == FieldHighlight::Type) {
                FieldHighlight *fhl = static_cast<FieldHighlight *>(hl);
                if (fhl->file() == 10) {
                    QRect r = calculateSquareRectangleBasedOnBoardState(fhl->file(), 10 - fhl->rank());
                    QRect rect = QRect(r.left() + m_param2 + m_squareSize + m_offsetX, r.top() + m_offsetY, r.width(), r.height());
                    painter->fillRect(rect, fhl->color());
                } else if (fhl->file() == 11) {
                    QRect r = calculateSquareRectangleBasedOnBoardState(fhl->file(), fhl->rank());
                    QRect rect = QRect(r.left() + m_squareSize * 10 - m_param2 + m_offsetX, m_squareSize * 8 - r.top() + m_offsetY, r.width(), r.height());
                    painter->fillRect(rect, fhl->color());
                } else {
                    QRect r = calculateSquareRectangleBasedOnBoardState(fhl->file(), fhl->rank());
                    QRect rect = QRect(r.left() + m_offsetX, r.top() + m_offsetY, r.width(), r.height());
                    painter->fillRect(rect, fhl->color());
                }
            }
        }
    }
}

// クリックした位置を基に、マスを特定する。
QPoint ShogiView::getClickedSquare(const QPoint &clickPosition) const
{
    if (m_flipMode) {
        return getClickedSquareInFlippedState(clickPosition);
    } else {
        return getClickedSquareInDefaultState(clickPosition);
    }
}

// クリックした位置を基に、通常モードでのマスを特定する。
QPoint ShogiView::getClickedSquareInDefaultState(const QPoint& pos) const
{
    // 将棋盤がセットされていない場合、無効な位置(QPoint())を返す。
    if (!m_board) return QPoint();

    const QSize fs = fieldSize().isValid() ? fieldSize()
                                           : QSize(m_squareSize, m_squareSize);
    const int w = fs.width();
    const int h = fs.height();

    // 1) まず盤面のヒットテスト（整数演算）
    const QRect boardRect(m_offsetX, m_offsetY,
                          w * m_board->files(), h * m_board->ranks());

    if (boardRect.contains(pos)) {
        const int xIn = pos.x() - boardRect.left();
        const int yIn = pos.y() - boardRect.top();
        const int colFromLeft = xIn / w;      // 0..8
        const int rowFromTop  = yIn / h;      // 0..8

        const int file = m_board->files() - colFromLeft; // 9..1 → 1..9
        const int rank = rowFromTop + 1;                 // 1..9
        return QPoint(file, rank);
    }

    // 2) 盤面外なら既存の「駒台」判定（そのまま流用でOK）
    // --- ここに今までの駒台当たり判定ブロックを置く ---
    // 例：先手駒台（右）の判定など
    float tempFile = (pos.x() - m_param2 - m_offsetX) / float(w);
    float tempRank = (pos.y() - m_offsetY) / float(h);
    int   rank     = static_cast<int>(tempRank);
    if (m_positionEditMode) {
        if ((tempFile >= 0) && (tempFile < 1) && (rank >= 1) && (rank <= 8))
            return QPoint(10, m_board->ranks() - rank);
    } else {
        if ((tempFile >= 0) && (tempFile < 1) && (rank >= 2) && (rank <= 8))
            return QPoint(10, m_board->ranks() - rank);
    }

    tempFile = (pos.x() + m_param1 - m_offsetX) / float(w);
    tempRank = (pos.y() - m_offsetY) / float(h);
    int file = static_cast<int>(tempFile);
    rank     = static_cast<int>(tempRank);
    if (m_positionEditMode) {
        if ((file == 1) && (rank >= 0) && (rank <= 7))
            return QPoint(11, m_board->ranks() - rank);
    } else {
        if ((file == 1) && (rank >= 0) && (rank <= 6))
            return QPoint(11, m_board->ranks() - rank);
    }

    return QPoint(); // 何もヒットしない
}

QPoint ShogiView::getClickedSquareInFlippedState(const QPoint& pos) const
{
    if (!m_board) return QPoint();

    const QSize fs = fieldSize().isValid() ? fieldSize()
                                           : QSize(m_squareSize, m_squareSize);
    const int w = fs.width();
    const int h = fs.height();

    // 1) まず盤面ヒット（整数演算）
    const QRect boardRect(m_offsetX, m_offsetY,
                          w * m_board->files(), h * m_board->ranks());

    if (boardRect.contains(pos)) {
        const int xIn = pos.x() - boardRect.left();
        const int yIn = pos.y() - boardRect.top();
        const int colFromLeft = xIn / w;      // 0..8
        const int rowFromTop  = yIn / h;      // 0..8

        const int file = colFromLeft + 1;                     // 1..9
        const int rank = m_board->ranks() - rowFromTop;       // 9..1
        return QPoint(file, rank);
    }

    // 2) 盤面外なら既存の駒台判定（そのまま流用）
    float tempFile = (pos.x() - m_param2 - m_offsetX) / float(w);
    float tempRank = (pos.y() - m_offsetY) / float(h);
    int   rank     = static_cast<int>(tempRank);
    if (m_positionEditMode) {
        if ((tempFile >= 0) && (tempFile < 1) && (rank >= 1) && (rank <= 8))
            return QPoint(11, rank + 1);
    } else {
        if ((tempFile >= 0) && (tempFile < 1) && (rank >= 2) && (rank <= 8))
            return QPoint(11, rank + 1);
    }

    tempFile = (pos.x() + m_param1 - m_offsetX) / float(w);
    tempRank = (pos.y() - m_offsetY) / float(h);
    rank     = static_cast<int>(tempRank);
    if (m_positionEditMode) {
        if ((tempFile >= 1) && (tempFile < 2) && (rank >= 0) && (rank <= 7))
            return QPoint(10, rank + 1);
    } else {
        if ((tempFile >= 1) && (tempFile < 2) && (rank >= 0) && (rank <= 6))
            return QPoint(10, rank + 1);
    }

    return QPoint();
}

// マウスボタンがクリックされた時のイベント処理を行う。
// 左クリックされた場合は、その位置にある将棋盤上のマスを特定し、clickedシグナルを発行する。
// 右クリックされた場合は、同様にマスを特定し、rightClickedシグナルを発行する。
void ShogiView::mouseReleaseEvent(QMouseEvent *event)
{
    // 駒を選択中に右クリックすると選択をキャンセルする。
    if (m_dragging && event->button() == Qt::RightButton) {
        // ドラッグ中の状態を終了する。
        endDrag();

        // イベントの位置にあるマスを特定する。
        QPoint pt = getClickedSquare(event->pos());

        // マスが右クリックされたことを通知するシグナルを発行する。
        emit rightClicked(pt);

        return;
    }

    // 左クリックの場合
    if (event->button() == Qt::LeftButton) {
        // イベントの位置にあるマスを特定する。
        QPoint pt = getClickedSquare(event->pos());

        // マスが有効でない場合（将棋盤外など）は何もしない。
        if (pt.isNull()) return;

        // エラーが発生している場合は何もしない。
        if (m_errorOccurred) return;

        // clickedシグナルを発行し、マスの位置を通知する。
        emit clicked(pt);
    }
    // 右クリックの場合
    else if (event->button() == Qt::RightButton) {
        // イベントの位置にあるマスを特定する。
        QPoint pt = getClickedSquare(event->pos());

        // マスが有効でない場合（将棋盤外など）は何もしない。
        if (pt.isNull()) return;

        // rightClickedシグナルを発行し、マスの位置を通知する。
        emit rightClicked(pt);
    }
}

// 指定されたハイライトオブジェクトをハイライトリストに追加し、表示を更新する。
void ShogiView::addHighlight(ShogiView::Highlight* hl)
{
    m_highlights.append(hl);

    update();
}

// 指定されたハイライトオブジェクトをハイライトリストから削除し、表示を更新する。
void ShogiView::removeHighlight(ShogiView::Highlight* hl)
{
    m_highlights.removeOne(hl);

    update();
}

// ハイライトリストから全てのデータを削除し、表示を更新する。
void ShogiView::removeHighlightAllData()
{
    m_highlights.clear();

    update();
}

// 先手側が画面手前の通常モード
// 将棋の駒画像を各駒文字（1文字）にセットする。
// 駒文字と駒画像をm_piecesに格納する。
// m_piecesの型はQMap<char, QIcon>
void ShogiView::setPieces()
{
    setPiece('P', QIcon(":/pieces/Sente_fu45.svg")); // 先手の歩
    setPiece('L', QIcon(":/pieces/Sente_kyou45.svg")); // 先手の香車
    setPiece('N', QIcon(":/pieces/Sente_kei45.svg")); // 先手の桂馬
    setPiece('S', QIcon(":/pieces/Sente_gin45.svg")); // 先手の銀
    setPiece('G', QIcon(":/pieces/Sente_kin45.svg")); // 先手の金
    setPiece('B', QIcon(":/pieces/Sente_kaku45.svg")); // 先手の角
    setPiece('R', QIcon(":/pieces/Sente_hi45.svg")); // 先手の飛車
    setPiece('K', QIcon(":/pieces/Sente_ou45.svg")); // 先手の王
    setPiece('Q', QIcon(":/pieces/Sente_to45.svg")); // 先手のと金
    setPiece('M', QIcon(":/pieces/Sente_narikyou45.svg")); // 先手の成香
    setPiece('O', QIcon(":/pieces/Sente_narikei45.svg")); // 先手の成桂
    setPiece('T', QIcon(":/pieces/Sente_narigin45.svg")); // 先手の成銀
    setPiece('C', QIcon(":/pieces/Sente_uma45.svg")); // 先手の馬
    setPiece('U', QIcon(":/pieces/Sente_ryuu45.svg")); // 先手の龍

    setPiece('p', QIcon(":/pieces/Gote_fu45.svg")); // 後手の歩
    setPiece('l', QIcon(":/pieces/Gote_kyou45.svg")); // 後手の香車
    setPiece('n', QIcon(":/pieces/Gote_kei45.svg")); // 後手の桂馬
    setPiece('s', QIcon(":/pieces/Gote_gin45.svg")); // 後手の銀
    setPiece('g', QIcon(":/pieces/Gote_kin45.svg")); // 後手の金
    setPiece('b', QIcon(":/pieces/Gote_kaku45.svg")); // 後手の角
    setPiece('r', QIcon(":/pieces/Gote_hi45.svg")); // 後手の飛車
    setPiece('k', QIcon(":/pieces/Gote_gyoku45.svg")); // 後手の玉
    setPiece('q', QIcon(":/pieces/Gote_to45.svg")); // 後手のと金
    setPiece('m', QIcon(":/pieces/Gote_narikyou45.svg")); // 後手の成香
    setPiece('o', QIcon(":/pieces/Gote_narikei45.svg")); // 後手の成桂
    setPiece('t', QIcon(":/pieces/Gote_narigin45.svg")); // 後手の成銀
    setPiece('c', QIcon(":/pieces/Gote_uma45.svg")); // 後手の馬
    setPiece('u', QIcon(":/pieces/Gote_ryuu45.svg")); // 後手の龍
}

// 後手側が画面手前の反転モード
// 将棋の駒画像を各駒文字（1文字）にセットする。
// 駒文字と駒画像をm_piecesに格納する。
// m_piecesの型はQMap<char, QIcon>
void ShogiView::setPiecesFlip()
{
    setPiece('p', QIcon(":/pieces/Sente_fu45.svg")); // 後手の歩
    setPiece('l', QIcon(":/pieces/Sente_kyou45.svg")); // 後手の香車
    setPiece('n', QIcon(":/pieces/Sente_kei45.svg")); // 後手の桂馬
    setPiece('s', QIcon(":/pieces/Sente_gin45.svg")); // 後手の銀
    setPiece('g', QIcon(":/pieces/Sente_kin45.svg")); // 後手の金
    setPiece('b', QIcon(":/pieces/Sente_kaku45.svg")); // 後手の角
    setPiece('r', QIcon(":/pieces/Sente_hi45.svg")); // 後手の飛車
    setPiece('k', QIcon(":/pieces/Sente_gyoku45.svg")); // 後手の玉
    setPiece('q', QIcon(":/pieces/Sente_to45.svg")); // 後手のと金
    setPiece('m', QIcon(":/pieces/Sente_narikyou45.svg")); // 後手の成香
    setPiece('o', QIcon(":/pieces/Sente_narikei45.svg")); // 後手の成桂
    setPiece('t', QIcon(":/pieces/Sente_narigin45.svg")); // 後手の成銀
    setPiece('c', QIcon(":/pieces/Sente_uma45.svg")); // 後手の馬
    setPiece('u', QIcon(":/pieces/Sente_ryuu45.svg")); // 後手の龍

    setPiece('P', QIcon(":/pieces/Gote_fu45.svg")); // 先手の歩
    setPiece('L', QIcon(":/pieces/Gote_kyou45.svg")); // 先手の香車
    setPiece('N', QIcon(":/pieces/Gote_kei45.svg")); // 先手の桂馬
    setPiece('S', QIcon(":/pieces/Gote_gin45.svg")); // 先手の銀
    setPiece('G', QIcon(":/pieces/Gote_kin45.svg")); // 先手の金
    setPiece('B', QIcon(":/pieces/Gote_kaku45.svg")); // 先手の角
    setPiece('R', QIcon(":/pieces/Gote_hi45.svg")); // 先手の飛車
    setPiece('K', QIcon(":/pieces/Gote_ou45.svg")); // 先手の王
    setPiece('Q', QIcon(":/pieces/Gote_to45.svg")); // 先手のと金
    setPiece('M', QIcon(":/pieces/Gote_narikyou45.svg")); // 先手の成香
    setPiece('O', QIcon(":/pieces/Gote_narikei45.svg")); // 先手の成桂
    setPiece('T', QIcon(":/pieces/Gote_narigin45.svg")); // 先手の成銀
    setPiece('C', QIcon(":/pieces/Gote_uma45.svg")); // 先手の馬
    setPiece('U', QIcon(":/pieces/Gote_ryuu45.svg")); // 先手の龍
}

// マウスクリックモードを設定する。
void ShogiView::setMouseClickMode(bool mouseClickMode)
{
    m_mouseClickMode = mouseClickMode;
}

// 盤面上の各マスのサイズをピクセル単位で返す。
int ShogiView::squareSize() const
{
    return m_squareSize;
}

// 盤面のサイズを拡大
void ShogiView::enlargeBoard()
{
    m_squareSize++;
    recalcLayoutParams();                     // ← 追加
    setFieldSize(QSize(m_squareSize, m_squareSize));
    updateBlackClockLabelGeometry();
    updateWhiteClockLabelGeometry();
    update();
}

// 盤面のサイズを縮小
void ShogiView::reduceBoard()
{
    m_squareSize--;
    recalcLayoutParams();                     // ← 追加
    setFieldSize(QSize(m_squareSize, m_squareSize));
    updateBlackClockLabelGeometry();
    updateWhiteClockLabelGeometry();
    update();
}

// エラーが発生したかどうかを示すフラグを設定する。
void ShogiView::setErrorOccurred(bool newErrorOccurred)
{
    m_errorOccurred = newErrorOccurred;
}

// 局面編集モードを設定する。
void ShogiView::setPositionEditMode(bool positionEditMode)
{   
    if (m_positionEditMode == positionEditMode) return;

    m_positionEditMode = positionEditMode;

    // 念のためここでも切り替え
    if (m_blackClockLabel) m_blackClockLabel->setVisible(!m_positionEditMode);
    if (m_blackNameLabel)  m_blackNameLabel->setVisible(!m_positionEditMode);

    updateBlackClockLabelGeometry();
    updateWhiteClockLabelGeometry();
}

// すき間（マス単位）を変えるAPI（例：setStandGapCols(0.5) で0.5マス）
void ShogiView::setStandGapCols(double cols)
{
    m_standGapCols = qBound(0.0, cols, 2.0);  // 0〜2マスの範囲にクランプ
    recalcLayoutParams();
    updateGeometry();
    updateBlackClockLabelGeometry();
    updateWhiteClockLabelGeometry();
    update();
}

// 局面編集モードで先手駒台に置かれた駒を描画する。
void ShogiView::drawEditModeBlackStandPiece(QPainter* painter, const int file, const int rank) const
{
    // 駒台のマスの位置を計算
    const QRect fieldRect = calculateSquareRectangleBasedOnBoardState(file, rank);

    // 描画位置を調整
    QRect adjustedRect;

    if (m_flipMode) {
        // 盤面が反転している場合、位置を左側に調整
        adjustedRect.setRect(fieldRect.left() - m_param1 + m_offsetX, fieldRect.top() + m_offsetY, fieldRect.width(), fieldRect.height());
    } else {
        // 盤面が反転していない場合、位置を右側に調整
        adjustedRect.setRect(fieldRect.left() + m_param1 + m_offsetX, fieldRect.top() + m_offsetY, fieldRect.width(), fieldRect.height());
    }

    // 選択した駒台の駒文字を取得する。
    QChar value = rankToBlackShogiPiece(rank);

    // 駒台の駒画像を描画する。
    if (value != ' ') {
        QIcon icon = piece(value);
        if (!icon.isNull()) {
            icon.paint(painter, adjustedRect, Qt::AlignCenter);
        }
    }
}

// 局面編集モードで先手駒台に置かれた駒の枚数を描画する。
void ShogiView::drawEditModeBlackStandPieceCount(QPainter* painter, const int file, const int rank) const
{
    // 駒台のマスの位置を計算
    const QRect fieldRect = calculateSquareRectangleBasedOnBoardState(file, rank);

    // 描画位置を調整
    QRect adjustedRect;

    if (m_flipMode) {
        // 盤面が反転している場合、位置を左側に調整
        adjustedRect.setRect(fieldRect.left() - m_param1 + m_offsetX, fieldRect.top() + m_offsetY, fieldRect.width(), fieldRect.height());
    } else {
        // 盤面が反転していない場合、位置を右側に調整
        adjustedRect.setRect(fieldRect.left() + m_param1 + m_offsetX, fieldRect.top() + m_offsetY, fieldRect.width(), fieldRect.height());
    }

    // 選択した駒台の駒文字を取得する。
    QChar value = rankToBlackShogiPiece(rank);

    // 駒台の持ち駒の枚数を描画する。
    drawStandPieceCount(painter, adjustedRect, value);
}

// 局面編集モードで後手駒台に置かれた駒を描画する。
void ShogiView::drawEditModeWhiteStandPiece(QPainter* painter, const int file, const int rank) const
{
    // 駒台のマスの位置を計算
    const QRect fieldRect = calculateSquareRectangleBasedOnBoardState(file, rank);

    // 描画位置を調整
    QRect adjustedRect;

    if (m_flipMode) {
        // 盤面が反転している場合、位置を右側に調整
        adjustedRect.setRect(fieldRect.left() + m_param2 + m_offsetX, fieldRect.top() + m_offsetY, fieldRect.width(), fieldRect.height());
    } else {
        // 盤面が反転していない場合、位置を左側に調整
        adjustedRect.setRect(fieldRect.left() - m_param2 + m_offsetX, fieldRect.top() + m_offsetY, fieldRect.width(), fieldRect.height());
    }

    // 選択した駒台の駒文字を取得する。
    QChar value = rankToWhiteShogiPiece(rank);

    // 駒台の駒画像を描画する。
    if (value != ' ') {
        QIcon icon = piece(value);
        if (!icon.isNull()) {
            icon.paint(painter, adjustedRect, Qt::AlignCenter);
        }
    }
}

// 局面編集モードで後手駒台に置かれた駒の枚数を描画する。
void ShogiView::drawEditModeWhiteStandPieceCount(QPainter* painter, const int file, const int rank) const
{
    // 駒台のマスの位置を計算
    const QRect fieldRect = calculateSquareRectangleBasedOnBoardState(file, rank);

    // 描画位置を調整
    QRect adjustedRect;

    if (m_flipMode) {
        // 盤面が反転している場合、位置を右側に調整
        adjustedRect.setRect(fieldRect.left() + m_param2 + m_offsetX, fieldRect.top() + m_offsetY, fieldRect.width(), fieldRect.height());
    } else {
        // 盤面が反転していない場合、位置を左側に調整
        adjustedRect.setRect(fieldRect.left() - m_param2 + m_offsetX, fieldRect.top() + m_offsetY, fieldRect.width(), fieldRect.height());
    }

    // 選択した駒台の駒文字を取得する。
    QChar value = rankToWhiteShogiPiece(rank);

    // 駒台の持ち駒の枚数を描画する。
    drawStandPieceCount(painter, adjustedRect, value);
}

// 将棋盤を反転させるフラグのsetter
void ShogiView::setFlipMode(bool newFlipMode)
{
    m_flipMode = newFlipMode;

    // ▲▽△▼記号を付け替える。
    refreshNameLabels();

    updateBlackClockLabelGeometry();
    updateWhiteClockLabelGeometry();
}

// 将棋盤を反転させるフラグを返す。
bool ShogiView::getFlipMode() const
{
    return m_flipMode;
}

// 「全ての駒を駒台へ」をクリックした時に実行される。
// 先手と後手の駒を同じ枚数にして全ての駒を駒台に載せる。
void ShogiView::resetAndEqualizePiecesOnStands()
{
    // ハイライトリストから全てのデータを削除し、表示を更新する。
    removeHighlightAllData();

    // 先手と後手の駒を同じ枚数にして全ての駒を駒台に載せる。
    board()->resetGameBoard();

    // 表示を更新する。
    update();
}

// 平手初期局面に盤面を初期化する。
void ShogiView::initializeToFlatStartingPosition()
{
    // ハイライトリストから全てのデータを削除し、表示を更新する。
    removeHighlightAllData();

    // 平手初期局面のsfen文字列
    QString flatInitialSFENStr = "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1";

    // 将棋盤と駒台のSFEN文字列を指定して将棋盤と駒台の駒の更新を行い、再描画する。
    board()->setSfen(flatInitialSFENStr);

    // 駒台の各駒の数を全て0にする。
    board()->initStand();

    // 表示を更新する。
    update();
}

// 詰将棋の初期局面に盤面を初期化する。
void ShogiView::shogiProblemInitialPosition()
{
    // ハイライトリストから全てのデータを削除し、表示を更新する。
    removeHighlightAllData();

    // 詰将棋の初期局面のsfen文字列
    //QString shogiProblemInitialSFENStr = "4k4/9/9/9/9/9/9/9/9 b 2r2b4g4s4n4l18p 1";
    QString shogiProblemInitialSFENStr = "5+r1kl/6p2/6Bpn/9/7P1/9/9/9/9 b RSb4g3s3n3l15p 1";

    // 将棋盤と駒台のSFEN文字列を指定して将棋盤と駒台の駒の更新を行い、再描画する。
    board()->setSfen(shogiProblemInitialSFENStr);

    // 先手の駒台の玉の数を1にする。
    board()->incrementPieceOnStand('K');

    // 表示を更新する。
    update();
}

// 先手の配置を後手の配置に変更し、後手の配置を先手の配置に変更する。
void ShogiView::flipBoardSides()
{
     // ハイライトリストから全てのデータを削除し、表示を更新する。
    removeHighlightAllData();

    // 先手の配置を後手の配置に変更し、後手の配置を先手の配置に変更する。
    board()->flipSides();

    // 表示を更新する。
    update();
}

// 指定された段に対応する先手の駒の種類を返す。
QChar ShogiView::rankToBlackShogiPiece(const int rank) const
{
    // 各段に対応する駒の文字をマッピングする辞書
    static const QMap<int, QChar> rankToPiece = {
        {2, 'K'}, // 王
        {3, 'R'}, // 飛車
        {4, 'B'}, // 角
        {5, 'G'}, // 金
        {6, 'S'}, // 銀
        {7, 'N'}, // 桂
        {8, 'L'}, // 香
        {9, 'P'}  // 歩
    };

    // 指定された段に対応する駒の文字を返す。
    return rankToPiece.value(rank, ' ');
}

// 指定された段に対応する後手の駒の種類を返す。
QChar ShogiView::rankToWhiteShogiPiece(const int rank) const
{
    // 各段に対応する駒の文字をマッピングする辞書
    static const QMap<int, QChar> rankToPiece = {
        {1, 'p'}, // 歩
        {2, 'l'}, // 香
        {3, 'n'}, // 桂
        {4, 's'}, // 銀
        {5, 'g'}, // 金
        {6, 'b'}, // 角
        {7, 'r'}, // 飛車
        {8, 'k'}  // 玉
    };

    // 指定された段に対応する駒の文字を返す。
    return rankToPiece.value(rank, ' ');
}

// 対局時に指す駒を左クリックするとドラッグが開始される。
void ShogiView::startDrag(const QPoint &from)
{
    // 駒台から指す場合
    // MainWindow::onShogiViewClickedにもガードを入れている。
    // 持ち駒が1枚も無いマスを左クリックすると駒画像がドラッグされないようにする。
    // このチェックが無いと、持ち駒が1枚も無いマスを左クリックするとドラッグされてしまう。
    if ((from.x() == 10 || from.x() == 11)) {
        QChar piece = board()->getPieceCharacter(from.x(), from.y());

        // 持ち駒の枚数が0以下の場合はドラッグを開始しない。
        if (board()->m_pieceStand.value(piece) <= 0) return;
    }

    // ドラッグ状態のフラグをtrueに設定する。
    m_dragging  = true;

    // ドラッグ開始位置を設定する。
    m_dragFrom  = from;

    // 指す駒の駒文字を取得する。
    m_dragPiece = m_board->getPieceCharacter(from.x(), from.y());

    // ドラッグ位置を現在のマウスカーソル位置に設定する。
    m_dragPos   = mapFromGlobal(QCursor::pos());

    // ── ここから一時枚数マップの準備 ──
    m_tempPieceStandCounts = m_board->m_pieceStand;

    // 駒台なら枚数を1減らす。
    if (from.x() == 10 || from.x() == 11) {
        m_dragFromStand = true;
        m_tempPieceStandCounts[m_dragPiece]--;
    } else {
        m_dragFromStand = false;
    }

    // 盤面を更新する。
    update();
}

// ドラッグを終了する。
void ShogiView::endDrag()
{
    // ドラッグ状態のフラグをfalseに設定する。
    m_dragging = false;

    // 一時マップをクリア → 元の描画に戻る
    m_tempPieceStandCounts.clear();

    // 盤面を終了する。
    update();
}

// マウスボタンがクリックされた時のイベント処理を行う。
void ShogiView::mouseMoveEvent(QMouseEvent* event)
{
    // ドラッグ中の場合
    if (m_dragging) {
        // マウスの移動に合わせて駒位置を更新し、再描画する。
        m_dragPos = event->pos();
        update();
    }

    // ベースクラスのマウス移動イベント処理を呼び出し、ツールチップやスタイル更新などの標準動作を維持する。
    QWidget::mouseMoveEvent(event);
}

// 局面編集モードかどうかのフラグを返す。
bool ShogiView::positionEditMode() const
{
    return m_positionEditMode;
}

void ShogiView::resizeEvent(QResizeEvent* e) {
    updateBlackClockLabelGeometry();
    updateWhiteClockLabelGeometry();   // ★ 追加
    QWidget::resizeEvent(e);
}

QRect ShogiView::blackStandBoundingRect() const
{
    if (!m_board) return {};

    const QSize fs = fieldSize().isValid() ? fieldSize()
                                           : QSize(m_squareSize, m_squareSize);

    const int rows     = m_positionEditMode ? 8 : 7;
    const int topRank  = m_flipMode ? 9 : (m_positionEditMode ? 2 : 3);
    const int leftCol  = m_flipMode ? 1 : 2;

    const QRect cell = calculateSquareRectangleBasedOnBoardState(leftCol, topRank);

    const int x = (m_flipMode ? (cell.left() - m_param1 + m_offsetX)
                              : (cell.left() + m_param1 + m_offsetX));
    const int y = cell.top() + m_offsetY;
    const int w = fs.width() * 2;          // ← 横幅ちょうど2マス分
    const int h = fs.height() * rows;

    return QRect(x, y, w, h);
}

QRect ShogiView::whiteStandBoundingRect() const
{
    if (!m_board) return {};

    const QSize fs = fieldSize().isValid() ? fieldSize()
                                           : QSize(m_squareSize, m_squareSize);

    const int rows    = m_positionEditMode ? 8 : 7;
    // 白駒台は rank=1..7(通常) / 1..8(編集)。flip時は rank が大きい方が画面上側になる。
    const int topRank = m_flipMode ? (m_positionEditMode ? 8 : 7) : 1;
    // 左端の列（画面座標で左になる列）
    const int leftCol = m_flipMode ? 1 : 2;

    const QRect cell = calculateSquareRectangleBasedOnBoardState(leftCol, topRank);

    // 白駒台は通常は盤の左（-m_param2）、flip時は右（+m_param2）
    const int x = (m_flipMode ? (cell.left() + m_param2 + m_offsetX)
                              : (cell.left() - m_param2 + m_offsetX));
    const int y = cell.top() + m_offsetY;
    const int w = fs.width() * 2;
    const int h = fs.height() * rows;

    return QRect(x, y, w, h);
}

void ShogiView::updateBlackClockLabelGeometry()
{
    // 編集モードは非表示のまま
    if (m_positionEditMode) {
        if (m_blackClockLabel) m_blackClockLabel->hide();
        if (m_blackNameLabel)  m_blackNameLabel->hide();
        return;
    }

    if (!m_blackClockLabel || !m_blackNameLabel) return;

    const QRect stand = blackStandBoundingRect();
    if (!stand.isValid()) {
        m_blackClockLabel->hide();
        m_blackNameLabel->hide();
        return;
    }

    const QSize fs = fieldSize().isValid() ? fieldSize()
                                           : QSize(m_squareSize, m_squareSize);
    const int marginOuter = 4;   // 駒台とのスキマ
    const int marginInner = 2;   // 名前と時計のスキマ

    const int nameH  = qMax(int(fs.height()*0.8), fs.height());
    const int clockH = qMax(int(fs.height()*0.9), fs.height());

    const int x = stand.left();
    const int yAbove = stand.top() - (nameH + marginInner + clockH) - marginOuter;
    const int yBelow = stand.bottom() + 1 + marginOuter;

    QRect nameRect, clockRect;

    if (yAbove >= 0) {
        // 上側に置ける
        nameRect  = QRect(x, yAbove, stand.width(), nameH);
        clockRect = QRect(x, nameRect.bottom() + 1 + marginInner,
                          stand.width(), clockH);
    } else if (yBelow + nameH + marginInner + clockH <= height()) {
        // 上が無理なら下側に置く（★反転時はだいたいこちら）
        nameRect  = QRect(x, yBelow, stand.width(), nameH);
        clockRect = QRect(x, nameRect.bottom() + 1 + marginInner,
                          stand.width(), clockH);
    } else {
        // 上下どちらにも置けない → 最終手段として駒台内に収める
        nameRect  = QRect(x, stand.top() + marginOuter, stand.width(), nameH);
        clockRect = QRect(x, nameRect.bottom() + 1 + marginInner,
                          stand.width(), clockH);
        int overflow = (clockRect.bottom() + marginOuter) - stand.bottom();
        if (overflow > 0) {
            nameRect.translate(0, -overflow);
            clockRect.translate(0, -overflow);
        }
    }

    m_blackNameLabel->setGeometry(nameRect);
    m_blackClockLabel->setGeometry(clockRect);

    // フォント合わせ
    fitLabelFontToRect(m_blackClockLabel, m_blackClockLabel->text(), clockRect, 2);  
    QFont f = m_blackNameLabel->font();
    f.setPointSizeF(qMax(8.0, fs.height() * m_nameFontScale));
    m_blackNameLabel->setFont(f);

    m_blackNameLabel->raise();
    m_blackClockLabel->raise();
    m_blackNameLabel->show();
    m_blackClockLabel->show();
}

void ShogiView::updateWhiteClockLabelGeometry()
{
    // 編集モードは非表示
    if (m_positionEditMode) {
        if (m_whiteClockLabel) m_whiteClockLabel->hide();
        if (m_whiteNameLabel)  m_whiteNameLabel->hide();
        return;
    }

    if (!m_whiteClockLabel || !m_whiteNameLabel) return;

    const QRect stand = whiteStandBoundingRect();
    if (!stand.isValid()) {
        m_whiteClockLabel->hide();
        m_whiteNameLabel->hide();
        return;
    }

    const QSize fs = fieldSize().isValid() ? fieldSize()
                                           : QSize(m_squareSize, m_squareSize);

    const int marginOuter = 4;
    const int marginInner = 2;
    const int nameH  = qMax(int(fs.height()*0.8), fs.height());
    const int clockH = qMax(int(fs.height()*0.9), fs.height());

    const int x      = stand.left();
    const int yAbove = stand.top() - (nameH + marginInner + clockH) - marginOuter;
    const int yBelow = stand.bottom() + 1 + marginOuter;

    QRect nameRect, clockRect;

    if (yAbove >= 0) {
        // 上に置ける
        nameRect  = QRect(x, yAbove, stand.width(), nameH);
        clockRect = QRect(x, nameRect.bottom() + 1 + marginInner,
                          stand.width(), clockH);
    } else if (yBelow + nameH + marginInner + clockH <= height()) {
        // 上がムリなら下に置く
        nameRect  = QRect(x, yBelow, stand.width(), nameH);
        clockRect = QRect(x, nameRect.bottom() + 1 + marginInner,
                          stand.width(), clockH);
    } else {
        // 最終手段：駒台内に収める
        nameRect  = QRect(x, stand.top() + marginOuter, stand.width(), nameH);
        clockRect = QRect(x, nameRect.bottom() + 1 + marginInner,
                          stand.width(), clockH);
        int overflow = (clockRect.bottom() + marginOuter) - stand.bottom();
        if (overflow > 0) {
            nameRect.translate(0, -overflow);
            clockRect.translate(0, -overflow);
        }
    }

    m_whiteNameLabel->setGeometry(nameRect);
    m_whiteClockLabel->setGeometry(clockRect);

    // フォント合わせ
    fitLabelFontToRect(m_whiteClockLabel, m_whiteClockLabel->text(), clockRect, 2);
    QFont f = m_blackNameLabel->font();
    f.setPointSizeF(qMax(8.0, fs.height() * m_nameFontScale));
    m_whiteNameLabel->setFont(f);

    m_whiteNameLabel->raise();
    m_whiteClockLabel->raise();
    m_whiteNameLabel->show();
    m_whiteClockLabel->show();
}

void ShogiView::fitLabelFontToRect(QLabel* label, const QString& text,
                                   const QRect& rect, int paddingPx)
{
    if (!label) return;

    const QRect inner = rect.adjusted(paddingPx, paddingPx, -paddingPx, -paddingPx);

    // 二分探索で最大ポイントサイズを求める
    double lo = 6.0;           // 最小pt
    double hi = 200.0;         // 最大ptの上限（十分大きい値）
    QFont f = label->font();

    for (int i = 0; i < 18; ++i) { // 18回で十分収束
        const double mid = (lo + hi) * 0.5;
        f.setPointSizeF(mid);
        QFontMetrics fm(f);
        const int w = fm.horizontalAdvance(text);
        const int h = fm.height();

        if (w <= inner.width() && h <= inner.height()) {
            lo = mid; // まだ余裕→大きく
        } else {
            hi = mid; // はみ出し→小さく
        }
    }

    f.setPointSizeF(lo);
    label->setFont(f);
}

void ShogiView::setBlackClockText(const QString& text)
{
    if (!m_blackClockLabel) return;
    m_blackClockLabel->setText(text);
    fitLabelFontToRect(m_blackClockLabel, text, m_blackClockLabel->geometry(), 2);
}

QLabel *ShogiView::whiteClockLabel() const
{
    return m_whiteClockLabel;
}

void ShogiView::setWhiteClockText(const QString& text)
{
    if (!m_whiteClockLabel) return;
    m_whiteClockLabel->setText(text);
    fitLabelFontToRect(m_whiteClockLabel, text, m_whiteClockLabel->geometry(), 2);
}

void ShogiView::setNameFontScale(double scale)
{
    m_nameFontScale = std::clamp(scale, 0.2, 1.0);
    updateBlackClockLabelGeometry(); // 反映
}

// 記号除去（▲=U+25B2, ▼=U+25BC, ▽=U+25BD, △=U+25B3）
QString ShogiView::stripMarks(const QString& s)
{
    QString t = s;
    static const QChar marks[] = { QChar(0x25B2), QChar(0x25BC),
                                  QChar(0x25BD), QChar(0x25B3) };
    for (QChar m : marks) t.remove(m);
    return t.trimmed();
}

// 名前セットは「素の名前」を保存してから表示を更新
void ShogiView::setBlackPlayerName(const QString& name)
{
    //begin
    qDebug() << "in ShogiView::setBlackPlayerName";
    //end

    m_blackNameBase = stripMarks(name);
    refreshNameLabels();
}

void ShogiView::setWhitePlayerName(const QString& name)
{
    //begin
    qDebug() << "in ShogiView::setWhitePlayerName";
    //end

    m_whiteNameBase = stripMarks(name);
    refreshNameLabels();
}

// 矢印を m_flipMode に応じて付けて ElideLabel に反映
void ShogiView::refreshNameLabels()
{
    if (m_blackNameLabel) {
        const QString markBlack = m_flipMode ? QStringLiteral("▼")
                                             : QStringLiteral("▲");
        m_blackNameLabel->setFullText(markBlack + m_blackNameBase);

        // ★ 背景・文字色・枠線を明示して透過を防ぐ
        auto mkTip = [](const QString& plain) {
            return QStringLiteral(
                       "<div style='background-color:#FFF9C4; color:#333;"
                       "border:1px solid #C49B00; padding:6px; white-space:nowrap;'>%1</div>")
                .arg(plain.toHtmlEscaped());
        };
        m_blackNameLabel->setToolTip(mkTip(m_blackNameBase));
    }
    if (m_whiteNameLabel) {
        const QString markWhite = m_flipMode ? QStringLiteral("△")
                                             : QStringLiteral("▽");
        m_whiteNameLabel->setFullText(markWhite + m_whiteNameBase);

        // ★ 後手も同じく“背景つき”HTMLで固定
        auto mkTip = [](const QString& plain) {
            return QStringLiteral(
                       "<div style='background-color:#FFF9C4; color:#333;"
                       "border:1px solid #C49B00; padding:6px; white-space:nowrap;'>%1</div>")
                .arg(plain.toHtmlEscaped());
        };
        m_whiteNameLabel->setToolTip(mkTip(m_whiteNameBase));
    }

    qDebug() << "Black player name set to:" << m_blackNameBase;
    qDebug() << "White player name set to:" << m_whiteNameBase;
}

/*
void ShogiView::refreshNameLabels()
{
    if (m_blackNameLabel) {
        const QString markBlack = m_flipMode ? QStringLiteral("▼")
                                             : QStringLiteral("▲");
        m_blackNameLabel->setFullText(markBlack + m_blackNameBase);
        m_blackNameLabel->setToolTip(m_blackNameBase);
    }
    if (m_whiteNameLabel) {
        const QString markWhite = m_flipMode ? QStringLiteral("△")
                                             : QStringLiteral("▽");
        m_whiteNameLabel->setFullText(markWhite + m_whiteNameBase);
        m_whiteNameLabel->setToolTip(m_whiteNameBase);
    }

    //begin
    // デバッグ用：名前ラベルの更新を確認
    qDebug() << "Black player name set to:" << m_blackNameBase;
    qDebug() << "White player name set to:" << m_whiteNameBase;
    //end
}
*/

void ShogiView::recalcLayoutParams()
{
    constexpr int tweak = 0;

    // ラベル帯とフォントはやや控えめの係数に
    m_labelBandPx = std::max(10, int(m_squareSize * 0.68));   // 以前: 0.75〜0.9
    m_labelFontPt = std::clamp(m_squareSize * 0.26, 5.0, 18.0); // 以前: 0.38/20pt
    m_labelGapPx  = std::max(2,  int(m_squareSize * 0.12));   // 盤/駒台からの内側余白

    // 指定ギャップ（マス→px）
    const int userGapPx = qRound(m_squareSize * m_standGapCols);

    // 段番号が確実に入る最小ギャップ
    const int needGapPx = minGapForRankLabelsPx();

    // 実効ギャップ
    const int effGapPx   = std::max(userGapPx, needGapPx);
    const double effCols = double(effGapPx) / double(m_squareSize);

    // 盤位置・駒台位置
    m_param1     = qRound((2.0 + effCols) * m_squareSize) - tweak;
    m_param2     = qRound((9.0 + effCols) * m_squareSize) - tweak;
    m_offsetX    = m_param1 + tweak;
    m_standGapPx = qRound(effCols * m_squareSize);
}

void ShogiView::drawRank(QPainter* painter, const int rank) const
{
    if (!m_board) return;

    const QRect cell = calculateSquareRectangleBasedOnBoardState(1, rank);
    const int h = cell.height();
    const int y = cell.top() + m_offsetY;

    const bool rightSide = !m_flipMode;
    const int boardEdge  = rightSide ? boardRightPx() : boardLeftPx();
    const int innerEdge  = standInnerEdgePx(rightSide);
    const int xCenter    = (boardEdge + innerEdge) / 2;

    // 表示帯
    int w = m_labelBandPx;
    const int gapPx = std::abs(innerEdge - boardEdge);
    if (w > gapPx - 2) w = std::max(12, gapPx - 2);

    const QRect rankRect(xCenter - w/2, y, w, h);

    // ★ 段番号だけ少し小さめに
    QFont f = painter->font();
    double pt = m_labelFontPt * m_rankFontScale;  // 係数を掛ける
    // 帯の高さに対する上限も掛けてはみ出し防止
    pt = std::min(pt, h * 0.9);
    f.setPointSizeF(pt);
    painter->setFont(f);

    static const QStringList rankTexts = { "一","二","三","四","五","六","七","八","九" };
    if (rank >= 1 && rank <= rankTexts.size()) {
        painter->drawText(rankRect, Qt::AlignCenter, rankTexts.at(rank - 1));
    }
}

void ShogiView::drawFile(QPainter* painter, const int file) const
{
    if (!m_board) return;

    const QRect cell = calculateSquareRectangleBasedOnBoardState(file, 1);
    const int x = cell.left() + m_offsetX;
    const int w = cell.width();

    int h = m_labelBandPx;
    int y = 0;

    if (m_flipMode) {
        const int boardBottom = m_offsetY + m_squareSize * m_board->ranks();
        int avail = height() - boardBottom - 2;
        if (avail <= 0) return;
        h = std::min(h, std::max(8, avail - m_labelGapPx));
        y = boardBottom + m_labelGapPx;          // 盤からの内側余白
        if (y + h > height() - 1) y = height() - 1 - h;
    } else {
        int avail = m_offsetY - 2;
        if (avail <= 0) return;
        h = std::min(h, std::max(8, avail - m_labelGapPx));
        y = m_offsetY - m_labelGapPx - h;        // 盤からの内側余白
    }

    const QRect fileRect(x, y, w, h);

    QFont f = painter->font();
    f.setPointSizeF(std::min(m_labelFontPt, h * 0.75)); // 抑えめ
    painter->setFont(f);

    static const QStringList fileTexts = { "１","２","３","４","５","６","７","８","９" };
    if (file >= 1 && file <= fileTexts.size())
        painter->drawText(fileRect, Qt::AlignHCenter | Qt::AlignVCenter,
                          fileTexts.at(file - 1));
}

int ShogiView::minGapForRankLabelsPx() const
{
    const int bandH = std::max(10, int(m_squareSize * 0.68));

    QFont f = this->font();
    // 使いたいフォントサイズをそのまま採寸に使う
    f.setPointSizeF(std::clamp(m_labelFontPt, 5.0, double(bandH)));
    QFontMetrics fm(f);

    static const QStringList ranks = { "一","二","三","四","五","六","七","八","九" };

    int wMax = 0;
    for (const QString& s : ranks)
        wMax = std::max(wMax, fm.horizontalAdvance(s));

    const int padding = std::max(2, m_labelGapPx);      // 盤/駒台からの内側余白ぶん
    return wMax + padding * 2;                          // 文字幅 + 左右余白
}

// 盤端から駒台までの“内側端”を返す（片側の隙間 = m_standGapPx をそのまま使う）
int ShogiView::standInnerEdgePx(bool rightSide) const
{
    const int gap = m_standGapPx;  // recalcLayoutParams() で最小値が保証されている
    return rightSide ? (boardRightPx() + gap)
                     : (boardLeftPx()  - gap);
}

void ShogiView::setRankFontScale(double scale)
{
    // 50%〜120% の範囲でクランプ（お好みで調整可）
    m_rankFontScale = std::clamp(scale, 0.5, 1.2);
    update();   // 再描画
}

// 手番側の見た目を反映（ツールチップは触らない）
void ShogiView::applyTurnHighlight(bool blackActive)
{
    auto setOne = [&](QLabel* name, QLabel* clock, bool active) {
        // 背景は styleSheet（※ color は絶対に書かない）
        const QString on  = QString("background-color:%1;").arg(m_highlightBg.name());
        const QString off = QString("background:transparent;");

        if (name)  name ->setStyleSheet(active ? on : off);
        if (clock) clock->setStyleSheet(active ? on : off);

        // 文字色はパレット（QToolTip には波及しない）
        auto setFg = [&](QLabel* lbl){
            if (!lbl) return;
            QPalette p = lbl->palette();
            p.setColor(QPalette::WindowText, active ? m_highlightFgOn : m_highlightFgOff);
            lbl->setPalette(p);
        };
        setFg(name);
        setFg(clock);
    };

    setOne(m_blackNameLabel, m_blackClockLabel,  blackActive);
    setOne(m_whiteNameLabel, m_whiteClockLabel, !blackActive);
}

void ShogiView::setActiveSide(bool blackTurn)
{
    m_blackActive = blackTurn;
    applyTurnHighlight(m_blackActive);
}

void ShogiView::setHighlightStyle(const QColor& bgOn, const QColor& fgOn, const QColor& fgOff)
{
    m_highlightBg    = bgOn;
    m_highlightFgOn  = fgOn;
    m_highlightFgOff = fgOff;
    applyTurnHighlight(m_blackActive); // 現在の手番に再適用
}
