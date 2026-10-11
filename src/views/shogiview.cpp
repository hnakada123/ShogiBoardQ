/// @file shogiview.cpp
/// @brief 将棋盤面描画ビュークラスの実装（コア：コンストラクタ・状態管理）

#include "shogiview.h"
#include "shogiviewhighlighting.h"
#include "shogiboard.h"
#include "settingscommon.h"
#include "elidelabel.h"
#include "shogigamecontroller.h"
#include "logcategories.h"
#include "sfenutils.h"
#include "pieceimageprovider.h"
#include "boardappearance.h"

#include <QColor>
#include <QPainter>
#include <QSettings>
#include <QFont>
#include <QFontMetrics>
#include <QDebug>
#include <QSizePolicy>
#include <QLayout>
#include <QLabel>
#include <QProxyStyle>

namespace {

// 対局者名の吹き出しだけ、標準のツールチップより早く表示する。
class PlayerNameLabelStyle final : public QProxyStyle
{
public:
    explicit PlayerNameLabelStyle(QWidget* label)
    {
        setParent(label);
    }

    int styleHint(StyleHint hint, const QStyleOption* option, const QWidget* widget,
                  QStyleHintReturn* returnData) const override
    {
        if (hint == SH_ToolTip_WakeUpDelay) return 200;
        return QProxyStyle::styleHint(hint, option, widget, returnData);
    }
};

} // namespace

// Highlight基底クラスのデストラクタ（out-of-line定義でweak-vtables警告を回避）
ShogiView::Highlight::~Highlight() {}

// FieldHighlightクラスのデストラクタ
ShogiView::FieldHighlight::~FieldHighlight() {}

// コンストラクタ
ShogiView::ShogiView(QWidget *parent)
    : QWidget(parent),
    m_board(nullptr),
    m_errorOccurred(false),
    m_highlighting(new ShogiViewHighlighting(this, this))
{
    // ハイライト/矢印/手番表示の管理クラスを生成
    m_boardColors = BoardAppearance::instance().colors();
    m_boardVisuals = BoardAppearance::instance().visuals();
    connect(&BoardAppearance::instance(), &BoardAppearance::colorsChanged,
            this, &ShogiView::refreshBoardColors);
    connect(&BoardAppearance::instance(), &BoardAppearance::visualsChanged,
            this, &ShogiView::refreshBoardVisuals);
    connect(m_highlighting, &ShogiViewHighlighting::highlightsCleared,
            this, &ShogiView::highlightsCleared);

    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    QSettings settings(SettingsCommon::settingsFilePath(), QSettings::IniFormat);
    int sq = settings.value("SizeRelated/squareSize", 50).toInt();
    if (sq < 20 || sq > 150) sq = 50;
    m_layout.setSquareSize(sq);

    m_layout.setStandGapCols(0.7);
    recalcLayoutParams();

    setMouseTracking(true);

    // 盤面は背景の画像で自分の範囲をすべて塗るため、描き直しのたびに親の下地を塗らせない。
    setAttribute(Qt::WA_OpaquePaintEvent);

    // ───────────────────────────────── 時計・名前ラベル（先手：黒） ─────────────────────────────────
    m_blackClockLabel = new QLabel(QStringLiteral("00:00:00"), this);
    m_blackClockLabel->setObjectName(QStringLiteral("blackClockLabel"));
    m_blackClockLabel->setAlignment(Qt::AlignCenter);
    m_blackClockLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    m_blackClockLabel->setStyleSheet(QStringLiteral("background: transparent; color: black;"));
    {
        QFont f = font();
        f.setStyleName(QString());
        f.setItalic(false);
        f.setBold(true);
        f.setPointSizeF(qMax(8.0, m_layout.squareSize() * 0.45));
        m_blackClockLabel->setFont(f);
    }

    m_blackNameLabel = new ElideLabel(this);
    m_blackNameLabel->setObjectName(QStringLiteral("blackNameLabel"));
    m_blackNameLabel->setElideMode(Qt::ElideRight);
    m_blackNameLabel->setWordWrap(true);
    m_blackNameLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_blackNameLabel->setStyle(new PlayerNameLabelStyle(m_blackNameLabel));

    // ───────────────────────────────── 名前ラベル（後手：白） ─────────────────────────────────
    m_whiteNameLabel = new ElideLabel(this);
    m_whiteNameLabel->setObjectName(QStringLiteral("whiteNameLabel"));
    m_whiteNameLabel->setElideMode(Qt::ElideRight);
    m_whiteNameLabel->setWordWrap(true);
    m_whiteNameLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_whiteNameLabel->setStyle(new PlayerNameLabelStyle(m_whiteNameLabel));

    // ───────────────────────────────── 時計ラベル（後手：白） ─────────────────────────────────
    m_whiteClockLabel = new QLabel(QStringLiteral("00:00:00"), this);
    m_whiteClockLabel->setObjectName(QStringLiteral("whiteClockLabel"));
    m_whiteClockLabel->setAlignment(Qt::AlignCenter);
    m_whiteClockLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    m_whiteClockLabel->setStyleSheet(QStringLiteral("background: transparent; color: black;"));
    {
        QFont f = font();
        f.setStyleName(QString());
        f.setItalic(false);
        f.setBold(true);
        f.setPointSizeF(qMax(8.0, m_layout.squareSize() * 0.45));
        m_whiteClockLabel->setFont(f);
    }

    updateBlackClockLabelGeometry();
    updateWhiteClockLabelGeometry();

    // 起動直後の見た目を整える
    m_highlighting->applyStartupTypography();

    m_blackNameLabel->installEventFilter(this);
    m_whiteNameLabel->installEventFilter(this);

    connect(&PieceImageProvider::instance(), &PieceImageProvider::styleChanged,
            this, &ShogiView::refreshPieceImages);
    refreshPieceImages();
}

// ─────────────────────────────────────────────────────────────────────────────
// ボード接続
// ─────────────────────────────────────────────────────────────────────────────

void ShogiView::setBoard(ShogiBoard* board)
{
    qCDebug(lcView) << "setBoard called, old:" << m_board << "new:" << board;

    if (m_board == board) {
        qCDebug(lcView) << "setBoard: same board, skipping";
        return;
    }

    if (m_board) {
        QObject::disconnect(m_board, nullptr, this, nullptr);
    }

    m_board = board;
    invalidateFieldRectCache();
    // 新しい盤は、まだ画面に出ていないものとして扱う
    m_shownSquares = QList<std::optional<Piece>>(board ? board->files() * board->ranks() : 0);
    m_shownStandCounts.clear();

    if (board) {
        connect(board, &ShogiBoard::dataChanged, this, &ShogiView::onBoardSquareChanged);
        connect(board, &ShogiBoard::standChanged, this, &ShogiView::onBoardStandChanged);
        connect(board, &ShogiBoard::boardReset,  this, &ShogiView::onBoardReset);
        connect(board, &ShogiBoard::dataChanged, this, &ShogiView::positionChanged);
        connect(board, &ShogiBoard::boardReset, this, &ShogiView::positionChanged);
    }

    updateGeometry();

    qCDebug(lcView) << "setBoard complete, m_board now:" << m_board;
    relayoutTurnLabels();
    emit positionChanged();
}

// ─────────────────────────────────────────────────────────────────────────────
// ゲッター
// ─────────────────────────────────────────────────────────────────────────────

ShogiBoard* ShogiView::board() const
{
    return m_board;
}

QSize ShogiView::fieldSize() const
{
    return m_layout.fieldSize();
}

ElideLabel* ShogiView::blackNameLabel() const { return m_blackNameLabel; }
QLabel* ShogiView::blackClockLabel() const { return m_blackClockLabel; }
ElideLabel* ShogiView::whiteNameLabel() const { return m_whiteNameLabel; }
QLabel* ShogiView::whiteClockLabel() const { return m_whiteClockLabel; }

bool ShogiView::positionEditMode() const { return m_interaction.positionEditMode(); }
int  ShogiView::squareSize() const { return m_layout.squareSize(); }
bool ShogiView::flipMode() const { return m_layout.flipMode(); }

ShogiViewHighlighting* ShogiView::highlighting() const { return m_highlighting; }

// ─────────────────────────────────────────────────────────────────────────────
// サイズ系
// ─────────────────────────────────────────────────────────────────────────────

void ShogiView::setFieldSize(QSize fieldSize)
{
    if (m_layout.fieldSize() == fieldSize) {
        return;
    }

    m_layout.setFieldSize(fieldSize);
    invalidateFieldRectCache();

    emit fieldSizeChanged(fieldSize);

    updateGeometry();

    updateBlackClockLabelGeometry();
    updateWhiteClockLabelGeometry();
}

QSize ShogiView::sizeHint() const
{
    if (!m_board) {
        return QSize(100, 100);
    }

    return m_layout.viewSize(m_board->files(), m_board->ranks());
}

QSize ShogiView::minimumSizeHint() const
{
    ShogiViewLayout minimumLayout = m_layout;
    minimumLayout.setSquareSize(20);
    minimumLayout.recalcLayoutParams(font());
    return minimumLayout.viewSize(m_board ? m_board->files() : 9, m_board ? m_board->ranks() : 9);
}

// ─────────────────────────────────────────────────────────────────────────────
// 座標・矩形計算
// ─────────────────────────────────────────────────────────────────────────────

QRect ShogiView::calculateSquareRectangleBasedOnBoardState(const int file, const int rank) const
{
    if (!m_board) return QRect();
    return m_layout.calculateSquareRectangleBasedOnBoardState(
        file, rank, m_board->files(), m_board->ranks());
}

QRect ShogiView::calculateRectangleForRankOrFileLabel(const int file, const int rank) const
{
    if (!m_board) return QRect();
    return m_layout.calculateRectangleForRankOrFileLabel(file, rank, m_board->ranks());
}

void ShogiView::invalidateFieldRectCache()
{
    m_fieldRectCacheValid = false;
}

QRect ShogiView::cachedFieldRect(const int file, const int rank) const
{
    if (!m_board) return QRect();

    if (!m_fieldRectCacheValid) {
        m_fieldRectCache.clear();
        const int files = m_board->files();
        const int ranks = m_board->ranks();
        for (int f = 1; f <= files; ++f) {
            for (int r = 1; r <= ranks; ++r) {
                const auto key = static_cast<quint64>(f) << 8 | static_cast<quint64>(r);
                m_fieldRectCache.insert(key, m_layout.calculateSquareRectangleBasedOnBoardState(
                                            f, r, files, ranks));
            }
        }
        m_fieldRectCacheValid = true;
    }

    const auto key = static_cast<quint64>(file) << 8 | static_cast<quint64>(rank);
    return m_fieldRectCache.value(key);
}

int ShogiView::boardLeftPx() const { return m_layout.offsetX(); }

int ShogiView::boardRightPx() const {
    const int files = m_board ? m_board->files() : 9;
    return m_layout.boardRightPx(files);
}

int ShogiView::standInnerEdgePx(bool rightSide) const
{
    const int files = m_board ? m_board->files() : 9;
    return m_layout.standInnerEdgePx(rightSide, files);
}

QRect ShogiView::blackStandBoundingRect() const
{
    if (!m_board) return {};
    return m_layout.blackStandBoundingRect(m_board->files(), m_board->ranks());
}

QRect ShogiView::whiteStandBoundingRect() const
{
    if (!m_board) return {};
    return m_layout.whiteStandBoundingRect(m_board->files(), m_board->ranks());
}

void ShogiView::recalcLayoutParams()
{
    m_layout.recalcLayoutParams(font());
    invalidateFieldRectCache();
    relayoutTurnLabels();
}

void ShogiView::applyBoardScaleChange(bool emitSignal)
{
    invalidateFieldRectCache();
    recalcLayoutParams();
    updateGeometry();
    updateBlackClockLabelGeometry();
    updateWhiteClockLabelGeometry();

    if (emitSignal) {
        emit fieldSizeChanged(m_layout.fieldSize());
    }
    update();
}

// ─────────────────────────────────────────────────────────────────────────────
// ハイライト管理
// ─────────────────────────────────────────────────────────────────────────────

void ShogiView::addHighlight(Highlight* hl)    { m_highlighting->addHighlight(hl); }
void ShogiView::removeHighlight(Highlight* hl) { m_highlighting->removeHighlight(hl); }
void ShogiView::removeHighlightAllData()        { m_highlighting->removeHighlightAllData(); }
ShogiView::Highlight* ShogiView::highlight(int index) const { return m_highlighting->highlight(index); }
int ShogiView::highlightCount() const           { return m_highlighting->highlightCount(); }

// ─────────────────────────────────────────────────────────────────────────────
// 操作/状態切替
// ─────────────────────────────────────────────────────────────────────────────

void ShogiView::setMouseClickMode(bool mouseClickMode)
{
    m_interaction.setMouseClickMode(mouseClickMode);
}

void ShogiView::setSquareSize(int size)
{
    if (size < 20 || size > 150) {
        return;
    }
    m_layout.setSquareSize(size);
    applyBoardScaleChange(false);
}

void ShogiView::enlargeBoard(bool emitSignal)
{
    if (m_layout.squareSize() >= 150) {
        return;
    }

    m_layout.setSquareSize(m_layout.squareSize() + 1);
    applyBoardScaleChange(emitSignal);
}

void ShogiView::reduceBoard(bool emitSignal)
{
    if (m_layout.squareSize() <= 20) {
        return;
    }

    m_layout.setSquareSize(m_layout.squareSize() - 1);
    applyBoardScaleChange(emitSignal);
}

void ShogiView::setErrorOccurred(bool newErrorOccurred)
{
    m_errorOccurred = newErrorOccurred;
}

void ShogiView::setPositionEditMode(bool positionEditMode)
{
    if (m_interaction.positionEditMode() == positionEditMode) return;

    QLabel* tlBlack = this->findChild<QLabel*>(QStringLiteral("turnLabelBlack"));
    QLabel* tlWhite = this->findChild<QLabel*>(QStringLiteral("turnLabelWhite"));
    const bool blackTurnShown = (tlBlack && tlBlack->isVisible());
    const bool whiteTurnShown = (tlWhite && tlWhite->isVisible());

    m_interaction.setPositionEditMode(positionEditMode);

    relayoutTurnLabels();

    if (tlBlack) { tlBlack->setVisible(blackTurnShown); tlBlack->raise(); }
    if (tlWhite) { tlWhite->setVisible(whiteTurnShown); tlWhite->raise(); }

    update();
}

void ShogiView::setStandGapCols(double cols)
{
    m_layout.setStandGapCols(cols);
    recalcLayoutParams();
    updateGeometry();
    updateBlackClockLabelGeometry();
    updateWhiteClockLabelGeometry();
    update();
}

void ShogiView::setFlipMode(bool newFlipMode)
{
    if (m_layout.flipMode() == newFlipMode) return;

    QLabel* tlBlack = this->findChild<QLabel*>(QStringLiteral("turnLabelBlack"));
    QLabel* tlWhite = this->findChild<QLabel*>(QStringLiteral("turnLabelWhite"));
    const bool blackShown = (tlBlack && tlBlack->isVisible());
    const bool whiteShown = (tlWhite && tlWhite->isVisible());

    m_layout.setFlipMode(newFlipMode);
    invalidateFieldRectCache();
    refreshPieceImages();

    refreshNameLabels();

    updateBlackClockLabelGeometry();
    updateWhiteClockLabelGeometry();

    relayoutTurnLabels();

    if (tlBlack) { tlBlack->setVisible(blackShown); tlBlack->raise(); }
    if (tlWhite) { tlWhite->setVisible(whiteShown); tlWhite->raise(); }

    update();
}

void ShogiView::returnAllPiecesToBox()
{
    removeHighlightAllData();
    board()->resetGameBoard();
    update();
}

void ShogiView::initializeToFlatStartingPosition()
{
    removeHighlightAllData();
    board()->setSfen(SfenUtils::hirateSfen());

    board()->initStand();
    update();
}

void ShogiView::shogiProblemInitialPosition()
{
    removeHighlightAllData();

    QString shogiProblemInitialSFENStr =
        "5+r1kl/6p2/6Bpn/9/7P1/9/9/9/9 b RSb4g3s3n3l15p 1";
    board()->setSfen(shogiProblemInitialSFENStr);

    update();
}

void ShogiView::setClockEnabled(bool enabled)
{
    m_clockEnabled = enabled;
    relayoutTurnLabels();
}

// ─────────────────────────────────────────────────────────────────────────────
// 手番・ハイライトスタイル
// ─────────────────────────────────────────────────────────────────────────────

void ShogiView::setActiveSide(bool blackTurn)     { m_highlighting->setActiveSide(blackTurn); }
void ShogiView::setHighlightStyle(const QColor& bgOn, const QColor& fgOn, const QColor& fgOff)
{
    m_highlighting->setHighlightStyle(bgOn, fgOn, fgOff);
}

void ShogiView::setRankFontScale(double scale)
{
    m_layout.setRankFontScale(scale);
    update();
}

void ShogiView::updateBoardSize()
{
    setFieldSize(QSize(m_layout.squareSize(), qRound(m_layout.squareSize() * ShogiViewLayout::kSquareAspectRatio)));
    updateGeometry();
    relayoutTurnLabels();
}

void ShogiView::setUrgencyVisuals(Urgency u) { m_highlighting->setUrgencyVisuals(u); }

QImage ShogiView::toImage(qreal scale)
{
    QPixmap pm = this->grab(rect());
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

    // 棋譜をたどるたび・エンジンが指すたびに呼ばれる。局面の変化は ShogiBoard の通知で
    // 変わったマスだけを描き直すので、盤そのもの・マスの大きさが変わったときだけ全体を描き直す
    // （駒の画像が変わったときは loadPieceImages が描き直す）。
    loadPieceImages(m_layout.flipMode());

    const bool boardChanged = m_board != board;
    setBoard(board);

    const QSize field(squareSize(), qRound(squareSize() * ShogiViewLayout::kSquareAspectRatio));
    const bool fieldChanged = fieldSize() != field;
    setFieldSize(field);

    if (boardChanged || fieldChanged) update();
}

void ShogiView::configureFixedSizing(int squarePx)
{
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    const int s = (squarePx > 0) ? squarePx : squareSize();
    m_layout.setSquareSize(s);
    applyBoardScaleChange(false);
}

void ShogiView::applyClockUrgency(qint64 activeRemainMs)
{
    m_highlighting->applyClockUrgency(activeRemainMs);
}

void ShogiView::clearTurnHighlight()
{
    m_highlighting->clearTurnHighlight();
}
