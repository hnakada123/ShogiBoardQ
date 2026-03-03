/// @file test_stubs_sfen_collection.cpp
/// @brief SfenCollectionDialog テスト用スタブ
///
/// ShogiView / ElideLabel / ShogiViewLayout / ShogiViewInteraction の
/// 最小シンボルを提供し、本体の巨大な依存ツリーを回避する。

#include "shogiview.h"
#include "elidelabel.h"

// ===================== ShogiViewLayout stubs =====================
ShogiViewLayout::ShogiViewLayout() = default;
void ShogiViewLayout::recalcLayoutParams(const QFont&) {}
void ShogiViewLayout::fitBoardToWidget() {}
QRect ShogiViewLayout::calculateSquareRectangleBasedOnBoardState(int, int, int, int) const { return {}; }
QRect ShogiViewLayout::calculateRectangleForRankOrFileLabel(int, int, int) const { return {}; }
QRect ShogiViewLayout::blackStandBoundingRect(int, int) const { return {}; }
QRect ShogiViewLayout::whiteStandBoundingRect(int, int) const { return {}; }
int ShogiViewLayout::boardRightPx(int) const { return 0; }
int ShogiViewLayout::standInnerEdgePx(bool, int) const { return 0; }
int ShogiViewLayout::minGapForRankLabelsPx(const QFont&) const { return 0; }
void ShogiViewLayout::setStandGapCols(double) {}
void ShogiViewLayout::setNameFontScale(double) {}
void ShogiViewLayout::setRankFontScale(double) {}
void ShogiViewLayout::setFieldSize(const QSize&) {}
void ShogiViewLayout::setFlipMode(bool) {}
void ShogiViewLayout::setSquareSize(int) {}

// ===================== ShogiViewInteraction stubs =====================
ShogiViewInteraction::ShogiViewInteraction() {}
QPoint ShogiViewInteraction::getClickedSquare(const QPoint&, const ShogiViewLayout&, ShogiBoard*) const { return {}; }
QPoint ShogiViewInteraction::getClickedSquareInDefaultState(const QPoint&, const ShogiViewLayout&, ShogiBoard*) const { return {}; }
QPoint ShogiViewInteraction::getClickedSquareInFlippedState(const QPoint&, const ShogiViewLayout&, ShogiBoard*) const { return {}; }
void ShogiViewInteraction::startDrag(const QPoint&, ShogiBoard*, const QPoint&) {}
void ShogiViewInteraction::endDrag() {}
void ShogiViewInteraction::drawDraggingPiece(QPainter&, const ShogiViewLayout&, const QMap<QChar, QIcon>&) {}
void ShogiViewInteraction::updateDragPos(const QPoint&) {}
void ShogiViewInteraction::setMouseClickMode(bool) {}
void ShogiViewInteraction::setPositionEditMode(bool) {}

// ===================== ElideLabel stubs =====================
ElideLabel::ElideLabel(QWidget* parent) : QLabel(parent) {}
void ElideLabel::setFullText(const QString&) {}
QString ElideLabel::fullText() const { return {}; }
void ElideLabel::setElideMode(Qt::TextElideMode) {}
Qt::TextElideMode ElideLabel::elideMode() const { return Qt::ElideMiddle; }
void ElideLabel::setSlideOnHover(bool) {}
void ElideLabel::setSlideSpeed(int) {}
void ElideLabel::setSlideInterval(int) {}
void ElideLabel::setUnderline(bool) {}
void ElideLabel::setManualPanEnabled(bool) {}
QSize ElideLabel::sizeHint() const { return QLabel::sizeHint(); }
void ElideLabel::paintEvent(QPaintEvent* e) { QLabel::paintEvent(e); }
void ElideLabel::resizeEvent(QResizeEvent* e) { QLabel::resizeEvent(e); }
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
void ElideLabel::enterEvent(QEnterEvent* e) { QLabel::enterEvent(e); }
#else
void ElideLabel::enterEvent(QEvent* e) { QLabel::enterEvent(e); }
#endif
void ElideLabel::leaveEvent(QEvent* e) { QLabel::leaveEvent(e); }
void ElideLabel::mousePressEvent(QMouseEvent* e) { QLabel::mousePressEvent(e); }
void ElideLabel::mouseMoveEvent(QMouseEvent* e) { QLabel::mouseMoveEvent(e); }
void ElideLabel::mouseReleaseEvent(QMouseEvent* e) { QLabel::mouseReleaseEvent(e); }
void ElideLabel::onTimerTimeout() {}
void ElideLabel::updateElidedText() {}
void ElideLabel::startSlideIfNeeded() {}
void ElideLabel::stopSlide() {}
bool ElideLabel::isOverflowing() const { return false; }

// ===================== ShogiView stubs =====================
ShogiView::Highlight::~Highlight() = default;
ShogiView::FieldHighlight::~FieldHighlight() = default;

ShogiView::ShogiView(QWidget* parent) : QWidget(parent) {}

void ShogiView::setBoard(ShogiBoard*) {}
ShogiBoard* ShogiView::board() const { return nullptr; }
QSize ShogiView::fieldSize() const { return {}; }
QSize ShogiView::sizeHint() const { return {400, 500}; }
QSize ShogiView::minimumSizeHint() const { return {200, 250}; }
void ShogiView::updateBoardSize() {}
QRect ShogiView::calculateSquareRectangleBasedOnBoardState(int, int) const { return {}; }
QRect ShogiView::calculateRectangleForRankOrFileLabel(int, int) const { return {}; }
QRect ShogiView::cachedFieldRect(int, int) const { return {}; }
void ShogiView::invalidateFieldRectCache() {}
void ShogiView::setPiece(char, const QIcon&) {}
QIcon ShogiView::piece(QChar) const { return {}; }
void ShogiView::setPieces() {}
void ShogiView::setPiecesFlip() {}
QPoint ShogiView::getClickedSquare(const QPoint&) const { return {}; }
QPoint ShogiView::getClickedSquareInDefaultState(const QPoint&) const { return {}; }
QPoint ShogiView::getClickedSquareInFlippedState(const QPoint&) const { return {}; }
void ShogiView::addHighlight(Highlight*) {}
void ShogiView::removeHighlight(Highlight*) {}
void ShogiView::removeHighlightAllData() {}
ShogiView::Highlight* ShogiView::highlight(int) const { return nullptr; }
int ShogiView::highlightCount() const { return 0; }
void ShogiView::setArrows(const QList<Arrow>&) {}
void ShogiView::clearArrows() {}
void ShogiView::setMouseClickMode(bool) {}
int ShogiView::squareSize() const { return 50; }
void ShogiView::setSquareSize(int) {}
void ShogiView::setPositionEditMode(bool) {}
bool ShogiView::positionEditMode() const { return false; }
void ShogiView::resetAndEqualizePiecesOnStands() {}
void ShogiView::initializeToFlatStartingPosition() {}
void ShogiView::shogiProblemInitialPosition() {}
void ShogiView::flipBoardSides() {}
bool ShogiView::getFlipMode() const { return false; }
void ShogiView::setFlipMode(bool) {}
void ShogiView::setErrorOccurred(bool) {}
void ShogiView::startDrag(const QPoint&) {}
void ShogiView::endDrag() {}
void ShogiView::setBlackClockText(const QString&) {}
void ShogiView::setWhiteClockText(const QString&) {}
QLabel* ShogiView::blackClockLabel() const { return nullptr; }
QLabel* ShogiView::whiteClockLabel() const { return nullptr; }
void ShogiView::setClockEnabled(bool) {}
bool ShogiView::isClockEnabled() const { return false; }
void ShogiView::setBlackPlayerName(const QString&) {}
void ShogiView::setWhitePlayerName(const QString&) {}
ElideLabel* ShogiView::blackNameLabel() const { return nullptr; }
ElideLabel* ShogiView::whiteNameLabel() const { return nullptr; }
void ShogiView::setStandGapCols(double) {}
void ShogiView::setNameFontScale(double) {}
void ShogiView::setRankFontScale(double) {}
void ShogiView::setActiveSide(bool) {}
void ShogiView::setHighlightStyle(const QColor&, const QColor&, const QColor&) {}
void ShogiView::setBlackTimeMs(qint64) {}
void ShogiView::setWhiteTimeMs(qint64) {}
QImage ShogiView::toImage(qreal) { return {}; }
void ShogiView::applyBoardAndRender(ShogiBoard*) {}
void ShogiView::configureFixedSizing(int) {}
void ShogiView::clearTurnHighlight() {}
void ShogiView::setUiMuted(bool) {}
void ShogiView::setActiveIsBlack(bool) {}
void ShogiView::setGameOverStyleLock(bool) {}
ShogiViewHighlighting* ShogiView::highlighting() const { return nullptr; }
void ShogiView::setUrgencyVisuals(Urgency) {}
void ShogiView::updateTurnIndicator(ShogiGameController::Player) {}
void ShogiView::relayoutEditExitButton() {}
void ShogiView::setFieldSize(QSize) {}
void ShogiView::enlargeBoard(bool) {}
void ShogiView::reduceBoard(bool) {}
void ShogiView::applyClockUrgency(qint64) {}
void ShogiView::paintEvent(QPaintEvent*) {}
void ShogiView::mouseMoveEvent(QMouseEvent*) {}
void ShogiView::mouseReleaseEvent(QMouseEvent*) {}
void ShogiView::resizeEvent(QResizeEvent*) {}
void ShogiView::wheelEvent(QWheelEvent*) {}
bool ShogiView::eventFilter(QObject*, QEvent*) { return false; }
void ShogiView::fitBoardToWidget() {}
void ShogiView::drawFiles(QPainter*) {}
void ShogiView::drawFile(QPainter*, int) const {}
void ShogiView::drawRanks(QPainter*) {}
void ShogiView::drawRank(QPainter*, int) const {}
void ShogiView::drawBackground(QPainter*) {}
void ShogiView::drawBoardShadow(QPainter*) {}
void ShogiView::drawBoardMargin(QPainter*) {}
void ShogiView::drawStandShadow(QPainter*) {}
void ShogiView::drawBoardFields(QPainter*) {}
void ShogiView::drawField(QPainter*, int, int) const {}
void ShogiView::drawPieces(QPainter*) {}
void ShogiView::drawPiece(QPainter*, int, int) {}
void ShogiView::drawFourStars(QPainter*) {}
void ShogiView::drawBlackStandField(QPainter*, int, int) const {}
void ShogiView::drawWhiteStandField(QPainter*, int, int) const {}
void ShogiView::drawBlackStandPiece(QPainter*, int, int) const {}
void ShogiView::drawWhiteStandPiece(QPainter*, int, int) const {}
void ShogiView::drawBlackNormalModeStand(QPainter*) {}
void ShogiView::drawWhiteNormalModeStand(QPainter*) {}
void ShogiView::drawNormalModeStand(QPainter*) {}
void ShogiView::drawPiecesBlackStandInNormalMode(QPainter*) {}
void ShogiView::drawPiecesWhiteStandInNormalMode(QPainter*) {}
void ShogiView::drawPiecesStandFeatures(QPainter*) {}
void ShogiView::drawStandPieceIcon(QPainter*, const QRect&, QChar) const {}
QChar ShogiView::rankToBlackShogiPiece(int, int) const { return {}; }
QChar ShogiView::rankToWhiteShogiPiece(int, int) const { return {}; }
void ShogiView::updateBlackClockLabelGeometry() {}
void ShogiView::updateWhiteClockLabelGeometry() {}
void ShogiView::fitLabelFontToRect(QLabel*, const QString&, const QRect&, int) {}
QRect ShogiView::blackStandBoundingRect() const { return {}; }
QRect ShogiView::whiteStandBoundingRect() const { return {}; }
int ShogiView::boardLeftPx() const { return 0; }
int ShogiView::boardRightPx() const { return 0; }
int ShogiView::standInnerEdgePx(bool) const { return 0; }
void ShogiView::recalcLayoutParams() {}
void ShogiView::refreshNameLabels() {}
QString ShogiView::stripMarks(const QString& s) { return s; }
void ShogiView::ensureTurnLabels() {}
void ShogiView::relayoutTurnLabels() {}
void ShogiView::ensureAndPlaceEditExitButton() {}
void ShogiView::styleEditExitButton(QPushButton*) {}
void ShogiView::fitEditExitButtonFont(QPushButton*, int) {}

// ===================== ShogiGameController stubs =====================
#include "shogigamecontroller.h"
// ShogiGameController のメタオブジェクト/vtable シンボルが必要な場合のみ
// ここではヘッダーのenumのみ使用するので追加スタブは不要
