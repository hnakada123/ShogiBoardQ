#include "shogiview.h"
#include "shogiboard.h"
#include "enginesettingsconstants.h"
#include <QColor>
#include <QMouseEvent>
#include <QPainter>
#include <QSettings>
#include <QDir>
#include <QApplication>

using namespace EngineSettingsConstants;

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

    // 将棋盤を表すShogiBoardオブジェクトのポインタをnullptrで初期化する。
    m_board(nullptr)
{
    // カレントディレクトリをアプリケーションのディレクトリに設定する。
    QDir::setCurrent(QApplication::applicationDirPath());

    // 設定ファイルを読み込むためのQSettingsオブジェクトを初期化する。
    QSettings settings(SettingsFileName, QSettings::IniFormat);

    // マスのサイズを設定ファイルから読み込む。デフォルトは50。
    m_squareSize = settings.value("SizeRelated/squareSize", 50).toInt();

    m_param1 = m_squareSize * 3 - 10;
    m_param2 = m_squareSize * 10 - 10;

    // 盤を右にずらすオフセット
    m_offsetX = m_param1 + 10;

    // 盤を下にずらすオフセット
    m_offsetY = 30;
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

    // 将棋盤に段番号を描画する。
    drawRanks(&painter);

    // 将棋盤に筋番号を描画する。
    drawFiles(&painter);

    // 将棋盤の各マスを描画する。
    drawBoardFields(&painter);

    // 局面編集モードと通常モードに応じて駒台の描画処理を行う。
    drawEditModeFeatures(&painter);

    // 将棋盤に４つの星を描画する。
    drawFourStars(&painter);

    // マスをハイライトする。
    drawHighlights(&painter);

    // 駒を描画する。
    drawPieces(&painter);

    // エラーが発生した場合は描画処理を中断する。
    if (m_errorOccurred) return;

    // 駒台に置かれた駒とその数を描画する。
    drawPiecesEditModeStandFeatures(&painter);
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

// 盤面の指定された段（rank）に対応する段番号を描画する。
void ShogiView::drawRank(QPainter* painter, const int rank) const
{
    // 盤面の指定された段の矩形を計算する。
    const QRect fieldRect = calculateRectangleForRankOrFileLabel(1, rank);

    // 描画する段番号のテキスト位置を決定
    QRect rankRect;

    if (m_flipMode) {
        // 盤面が反転している場合、左側に段番号を描画する。
        rankRect.setRect(-40 + m_offsetX, fieldRect.top() + m_offsetY, fieldRect.left(), fieldRect.height());
    } else {
        // 盤面が反転していない場合、右側に段番号を描画する。
        rankRect.setRect(m_squareSize * 9 + m_offsetX - 10, fieldRect.top() + m_offsetY, fieldRect.left(), fieldRect.height());
    }

    // 段番号の文字列リスト（日本語の"一"から"九"まで）
    static const QStringList rankTexts = {"一", "二", "三", "四", "五", "六", "七", "八", "九"};

    // 有効な範囲内の段番号のみを描画する。
    if (rank >= 1 && rank <= rankTexts.size()) {
        painter->drawText(rankRect, Qt::AlignVCenter | Qt::AlignRight, rankTexts.at(rank - 1));
    }
}

// 盤面の指定された筋（file）に対応する筋番号を描画する。
void ShogiView::drawFile(QPainter* painter, const int file) const
{
    // 盤面の指定された筋の矩形を計算
    const QRect fieldRect = calculateSquareRectangleBasedOnBoardState(file, 1);

    QRect fileRect;

    // 描画する筋番号のテキスト位置を決定
    if (m_flipMode) {
        // 盤面が反転している場合、下側に筋番号を描画
        fileRect.setRect(fieldRect.left() + m_offsetX, m_offsetY + m_squareSize * 9 + 5, fieldRect.width(), height() - fieldRect.bottom());
    } else {
        // 盤面が反転していない場合、上側に筋番号を描画
        fileRect.setRect(fieldRect.left() + m_offsetX, m_offsetY - 25, fieldRect.width(), height() - fieldRect.bottom());
    }

    // 筋番号の文字列リスト（日本語の"１"から"９"まで）
    static const QStringList fileTexts = {"１", "２", "３", "４", "５", "６", "７", "８", "９"};

    // 有効な範囲内の筋番号のみを描画
    if (file >= 1 && file <= fileTexts.size()) {
        painter->drawText(fileRect, Qt::AlignHCenter | Qt::AlignTop, fileTexts.at(file - 1));
    }
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
    painter->setPen(palette().color(QPalette::Dark));

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

        emit errorOccurred(errorMessage);

        // エラーが発生した場合は描画処理を中断する。
        return;
    }

    // 駒が存在する場合、その駒のアイコンを描画
    if (value != ' ') {
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

    if (m_board->m_pieceStand[value] > 0) {
        if (value != ' ') {
            // 駒のアイコンを取得し、描画する。
            QIcon icon = piece(value);

            if (!icon.isNull()) {
                icon.paint(painter, adjustedRect, Qt::AlignCenter);
            }
        }
    }
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

    // 駒台にある駒の種類に応じて枚数を取得し、描画するテキストを設定
    QString pieceCountText;

    QChar value = rankToBlackShogiPiece(rank);

    if (m_board->m_pieceStand[value] > 0) {
        // 駒が存在する場合、その枚数をテキストとして設定
        pieceCountText = QString::number(m_board->m_pieceStand[value]);
    } else {
        // 駒が存在しない場合は何も描画しない
        pieceCountText = " ";
    }

    // 枚数のテキストを描画
    painter->drawText(adjustedRect, Qt::AlignVCenter | Qt::AlignCenter, pieceCountText);
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

    if (m_board->m_pieceStand[value] > 0) {
        QIcon icon = piece(value);
        if (!icon.isNull()) {
            icon.paint(painter, adjustedRect, Qt::AlignCenter);
        }
    }
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
    QString pieceCountText;

    QChar value = rankToWhiteShogiPiece(rank);

    if (m_board->m_pieceStand[value] > 0) {
        // 駒が存在する場合、その枚数をテキストとして設定
        pieceCountText = QString::number(m_board->m_pieceStand[value]);
    } else {
        // 駒が存在しない場合は何も描画しない
        pieceCountText = " ";
    }

    // 枚数のテキストを描画
    painter->drawText(adjustedRect, Qt::AlignVCenter | Qt::AlignCenter, pieceCountText);
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

// ユーザーがクリックした位置を基に、マスを特定する。
QPoint ShogiView::getClickedSquare(const QPoint &clickPosition) const
{
    if (m_flipMode) {
        return getClickedSquareInFlippedState(clickPosition);
    } else {
        return getClickedSquareInDefaultState(clickPosition);
    }
}

// ユーザーがクリックした位置を基に、通常モードでのマスを特定する。
QPoint ShogiView::getClickedSquareInDefaultState(const QPoint& clickPosition) const
{
    // 将棋盤がセットされていない場合、無効な位置(QPoint())を返す。
    if (!m_board) return QPoint();

    // 1マスのサイズを取得する。
    const QSize squareSize = fieldSize();

    // 浮動小数点で筋と段の仮の位置を計算するための変数
    float tempFile, tempRank;

    // 最終的に求める筋と段の整数値
    int file, rank;

    // 先手駒台の範囲を計算し、該当する場合は特殊な値を返す。
    // 例えば、QPoint(10, x)やQPoint(11, x)
    tempFile = (clickPosition.x() - m_param2 - m_offsetX) / squareSize.width();
    tempRank = (clickPosition.y() - m_offsetY) / squareSize.height();
    rank = static_cast<int>(tempRank);

    // 局面編集モードの場合
    if (m_positionEditMode) {
        if ((tempFile >= 0) && (tempFile < 1) && (rank >= 1) && (rank <= 8))
            // 先手駒台の特定の位置を返す。
            return QPoint(10, m_board->ranks() - rank);
    }
    // 通常モードの場合
    else {
        if ((tempFile >= 0) && (tempFile < 1) && (rank >= 2) && (rank <= 8))
            // 通常モードでも先手駒台の判定を行う。
            return QPoint(10, m_board->ranks() - rank);
    }

    // 後手駒台の範囲を計算し、該当する場合は特殊な値を返す。
    tempFile = (clickPosition.x() + m_param1 - m_offsetX) / squareSize.width();
    tempRank = (clickPosition.y() - m_offsetY) / squareSize.height();
    file = static_cast<int>(tempFile);
    rank = static_cast<int>(tempRank);

    // 局面編集モードの場合
    if (m_positionEditMode) {
        if ((file == 1) && (rank >= 0) && (rank <= 7))
            // 後手駒台の特定の位置を返す。
            return QPoint(11, m_board->ranks() - rank);
    }
    // 通常モードの場合
    else {
        if ((file == 1) && (rank >= 0) && (rank <= 6))
            // 通常モードでも後手駒台の判定を行う。
            return QPoint(11, m_board->ranks() - rank);
    }

    // 将棋盤内のマスの位置を計算する。
    tempFile = (clickPosition.x() - m_offsetX) / squareSize.width();
    tempRank = (clickPosition.y() - m_offsetY) / squareSize.height();
    file = static_cast<int>(tempFile);
    rank = static_cast<int>(tempRank);

    // クリックが将棋盤外であれば無効な位置(QPoint())を返す。
    if (tempFile < 0 || file >= m_board->files() || tempRank < 0 || rank >= m_board->ranks()) {
        return QPoint();
    }

    // 有効な将棋盤上のマスの位置(QPoint)を返す。将棋の筋は左から右へ、段は上から下へ数える。
    return QPoint(m_board->files() - file, rank + 1);
}

// ユーザーがクリックした位置を基に、盤面が反転している状態での将棋盤上のマスを特定する。
QPoint ShogiView::getClickedSquareInFlippedState(const QPoint& clickPosition) const
{
    // 将棋盤がセットされていない場合、無効な位置(QPoint())を返す。
    if (!m_board) return QPoint();

    // 1マスのサイズを取得する。
    const QSize squareSize = fieldSize();

    // 最終的に求める筋と段の整数値
    int file, rank;

    // 後手右の駒台の範囲を計算し、該当する場合は特殊な値を返す。
    // 例えば、QPoint(10, x)やQPoint(11, x)
    float tempFile = (clickPosition.x() - m_param2 - m_offsetX) / squareSize.width();
    float tempRank = (clickPosition.y() - m_offsetY) / squareSize.height();
    rank = static_cast<int>(tempRank);

    // 局面編集モードの場合
    if (m_positionEditMode) {
        if ((tempFile >= 0) && (tempFile < 1) && (rank >= 1) && (rank <= 8)) {
            // 後手駒台の特定の位置を返す。
            return QPoint(11, rank + 1);
        }
    } else {
        if ((tempFile >= 0) && (tempFile < 1) && (rank >= 2) && (rank <= 8)) {
            // 通常モードでも後手駒台の判定を行う。
            return QPoint(11, rank + 1);
        }
    }

    // 先手左の駒台の範囲を計算し、該当する場合は特殊な値を返す。
    tempFile = (clickPosition.x() + m_param1 - m_offsetX) / squareSize.width();
    tempRank = (clickPosition.y() - m_offsetY) / squareSize.height();
    rank = static_cast<int>(tempRank);

     // 局面編集モードの場合
    if (m_positionEditMode) {
        if ((tempFile >= 1) && (tempFile < 2) && (rank >= 0) && (rank <= 7)) {
            // 先手駒台の特定の位置を返す。
            return QPoint(10, rank + 1);
        }
    }
    // 通常モードの場合
    else {
        if ((tempFile >= 1) && (tempFile < 2) && (rank >= 0) && (rank <= 6)) {
            // 通常モードでも先手駒台の判定を行う。
            return QPoint(10, rank + 1);
        }
    }

    // 将棋盤内のマスの位置を計算する。
    tempFile = (clickPosition.x() - m_offsetX) / squareSize.width();
    tempRank = (clickPosition.y() - m_offsetY) / squareSize.height();
    file = static_cast<int>(tempFile);
    rank = static_cast<int>(tempRank);

    // クリックが将棋盤外であれば無効な位置(QPoint())を返す。
    if (tempFile < 0 || file >= m_board->files() || tempRank < 0 || rank >= m_board->ranks()) {
        return QPoint();
    }

    // 有効な将棋盤上のマスの位置(QPoint)を返す。この場合、筋の計算は反転させる。
    return QPoint(file + 1, m_board->ranks() - rank);
}

// マウスボタンがクリックされた時のイベント処理を行う。
// 左クリックされた場合は、その位置にある将棋盤上のマスを特定し、clickedシグナルを発行する。
// 右クリックされた場合は、同様にマスを特定し、rightClickedシグナルを発行する。
void ShogiView::mouseReleaseEvent(QMouseEvent *event)
{
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

// 盤面のサイズを拡大する。
void ShogiView::enlargeBoard()
{
    m_squareSize++;
    m_param1 = m_squareSize * 3 - 10;
    m_param2 = m_squareSize * 10 - 10;
    m_offsetX = m_param1 + 10;
    setFieldSize(QSize(m_squareSize, m_squareSize));

    // 表示を更新する。
    update();
}

// 盤面のサイズを縮小する。
void ShogiView::reduceBoard()
{
    m_squareSize--;
    m_param1 = m_squareSize * 3 - 10;
    m_param2 = m_squareSize * 10 - 10;
    m_offsetX = m_param1 + 10;
    setFieldSize(QSize(m_squareSize, m_squareSize));

    // 表示を更新する。
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
    m_positionEditMode = positionEditMode;
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

    // 駒台にある駒の種類を取得し、駒が存在する場合そのアイコンを描画
    QChar value = rankToBlackShogiPiece(rank);

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

    // 駒台にある駒の種類に応じて枚数を取得し、描画するテキストを設定
    QChar value = rankToBlackShogiPiece(rank);

    QString pieceCountText = QString::number(m_board->m_pieceStand[value]);

    // 枚数のテキストを描画
    painter->drawText(adjustedRect, Qt::AlignVCenter | Qt::AlignCenter, pieceCountText);
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

    // 駒台にある駒の種類を取得し、駒が存在する場合そのアイコンを描画
    QChar value = rankToWhiteShogiPiece(rank);

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

    // 駒台にある駒の種類に応じて枚数を取得し、描画するテキストを設定
    QChar value = rankToWhiteShogiPiece(rank);

    QString pieceCountText = QString::number(m_board->m_pieceStand[value]);

    // 枚数のテキストを描画
    painter->drawText(adjustedRect, Qt::AlignVCenter | Qt::AlignCenter, pieceCountText);
}

// 将棋盤を反転させるフラグのsetter
void ShogiView::setFlipMode(bool newFlipMode)
{
    m_flipMode = newFlipMode;
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
