/// @file pvboarddialog.cpp
/// @brief 読み筋盤面ダイアログクラスの実装

#include "pvboarddialog.h"
#include "pvboardcontroller.h"
#include "buttonstyles.h"
#include "shogiview.h"
#include "shogiboard.h"
#include "shogigamecontroller.h"
#include "analysissettings.h"
#include "dialogutils.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include "logcategories.h"
#include <QCloseEvent>
#include <QWheelEvent>
#include <QTimer>

namespace {
constexpr QSize kMinimumSize{400, 500};
} // namespace

PvBoardDialog::PvBoardDialog(const QString& baseSfen,
                             const QStringList& pvMoves,
                             QWidget* parent)
    : QDialog(parent)
    , m_controller(std::make_unique<PvBoardController>(baseSfen, pvMoves))
{
    setWindowTitle(tr("読み筋表示"));
    setMinimumSize(kMinimumSize);

    // 前回保存されたウィンドウサイズを読み込む
    DialogUtils::restoreDialogSize(this, AnalysisSettings::pvBoardDialogSize());

    // UIを構築（この中でm_boardが作られる）
    buildUi();

    // 初期盤面を設定
    const QString initialSfen = m_controller->currentSfen();
    if (!initialSfen.isEmpty()) {
        m_board->setSfen(initialSfen);
    }

    // ShogiViewの初期サイズを強制的に設定してレイアウトを確定
    m_shogiView->updateBoardSize();

    // updateBoardSize()が時計ラベルを再表示するため、再度非表示にする
    hideClockLabels();

    // 初期表示
    updateBoardDisplay();
    updateButtonStates();

    // レイアウト確定後に自動調整（起動直後のサイズ不足を防ぐ）
    QTimer::singleShot(0, this, &PvBoardDialog::adjustWindowToContents);
}

PvBoardDialog::~PvBoardDialog()
{
    // ハイライトをクリア
    clearMoveHighlights();
    // m_board と m_shogiView は Qt のオブジェクトツリーで管理
}

void PvBoardDialog::setKanjiPv(const QString& kanjiPv)
{
    m_controller->setKanjiPv(kanjiPv);
    if (m_pvLabel) {
        m_pvLabel->setText(kanjiPv);
    }
}

void PvBoardDialog::setPlayerNames(const QString& blackName, const QString& whiteName)
{
    m_blackPlayerName = blackName.isEmpty() ? tr("先手") : blackName;
    m_whitePlayerName = whiteName.isEmpty() ? tr("後手") : whiteName;

    if (m_shogiView) {
        m_shogiView->setBlackPlayerName(m_blackPlayerName);
        m_shogiView->setWhitePlayerName(m_whitePlayerName);
    }
}

void PvBoardDialog::setLastMove(const QString& lastMove)
{
    m_controller->setLastMove(lastMove);
    qCDebug(lcUi) << "setLastMove: lastMove=" << lastMove
             << " currentPly=" << m_controller->currentPly();
    if (m_controller->currentPly() == 0) {
        qCDebug(lcUi) << "setLastMove: calling updateMoveHighlights()";
        updateMoveHighlights();
    }
}

void PvBoardDialog::setPrevSfenForHighlight(const QString& prevSfen)
{
    m_controller->setPrevSfen(prevSfen);
    qCDebug(lcUi) << "setPrevSfenForHighlight: prevSfen=" << prevSfen.left(60);
    if (m_controller->currentPly() == 0) {
        qCDebug(lcUi) << "setPrevSfenForHighlight: calling updateMoveHighlights()";
        updateMoveHighlights();
    }
}

void PvBoardDialog::buildUi()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(8);

    // 読み筋ラベル（スクロール可能なラベル）
    m_pvLabel = new QLabel(this);
    m_pvLabel->setWordWrap(true);
    m_pvLabel->setStyleSheet(QStringLiteral(
        "QLabel { background-color: #f5f5f5; border: 1px solid #ccc; "
        "padding: 8px; font-size: 12px; }"));
    m_pvLabel->setMinimumHeight(60);
    m_pvLabel->setMaximumHeight(100);
    mainLayout->addWidget(m_pvLabel);

    // 将棋盤拡大・縮小ボタン（将棋盤の上に配置）
    QHBoxLayout* zoomLayout = new QHBoxLayout();
    zoomLayout->setSpacing(4);

    m_btnReduce = new QPushButton(QStringLiteral("将棋盤縮小 ➖"), this);
    m_btnReduce->setToolTip(tr("将棋盤を縮小する"));
    m_btnReduce->setStyleSheet(ButtonStyles::secondaryNeutral());
    connect(m_btnReduce, &QPushButton::clicked, this, &PvBoardDialog::onReduceBoard);
    zoomLayout->addWidget(m_btnReduce);

    m_btnEnlarge = new QPushButton(QStringLiteral("将棋盤拡大 ➕"), this);
    m_btnEnlarge->setToolTip(tr("将棋盤を拡大する"));
    m_btnEnlarge->setStyleSheet(ButtonStyles::secondaryNeutral());
    connect(m_btnEnlarge, &QPushButton::clicked, this, &PvBoardDialog::onEnlargeBoard);
    zoomLayout->addWidget(m_btnEnlarge);

    m_btnFlip = new QPushButton(tr("盤面の回転"), this);
    m_btnFlip->setToolTip(tr("盤面を回転する"));
    m_btnFlip->setStyleSheet(ButtonStyles::secondaryNeutral());
    connect(m_btnFlip, &QPushButton::clicked, this, &PvBoardDialog::onFlipBoard);
    zoomLayout->addWidget(m_btnFlip);

    zoomLayout->addStretch();
    mainLayout->addLayout(zoomLayout);

    // 将棋盤
    m_board = new ShogiBoard(9, 9, this);
    m_shogiView = new ShogiView(this);
    m_shogiView->setMouseClickMode(false);  // クリック操作は無効
    m_shogiView->setNameFontScale(0.30);    // GUI本体と同じフォントスケールを設定

    // 駒画像を読み込んでから盤を設定
    m_shogiView->setPieces();
    m_shogiView->setBoard(m_board);

    // ShogiViewのCtrl+ホイールイベントを横取りして、ダイアログ側で処理する
    m_shogiView->installEventFilter(this);

    // 対局者名をShogiViewのメソッドを使用（マーク付き表示）
    m_shogiView->setBlackPlayerName(m_blackPlayerName.isEmpty() ? tr("先手") : m_blackPlayerName);
    m_shogiView->setWhitePlayerName(m_whitePlayerName.isEmpty() ? tr("後手") : m_whitePlayerName);

    // ShogiViewをレイアウトに追加（サイズはShogiView自身のsizeHintに任せる）
    mainLayout->addWidget(m_shogiView, 1);

    // 手数ラベル
    m_plyLabel = new QLabel(this);
    m_plyLabel->setAlignment(Qt::AlignCenter);
    m_plyLabel->setStyleSheet(QStringLiteral(
        "QLabel { font-size: 14px; font-weight: bold; padding: 4px; }"));
    mainLayout->addWidget(m_plyLabel);

    // ナビゲーションボタン
    QHBoxLayout* btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(8);

    m_btnFirst = new QPushButton(QStringLiteral("⏮ 最初"), this);
    m_btnFirst->setMinimumWidth(80);
    m_btnFirst->setToolTip(tr("最初の局面に戻る"));
    m_btnFirst->setStyleSheet(ButtonStyles::wideNavigationButton());
    connect(m_btnFirst, &QPushButton::clicked, this, &PvBoardDialog::onGoFirst);
    btnLayout->addWidget(m_btnFirst);

    m_btnBack = new QPushButton(QStringLiteral("◀ 1手戻る"), this);
    m_btnBack->setMinimumWidth(100);
    m_btnBack->setToolTip(tr("1手戻る"));
    m_btnBack->setStyleSheet(ButtonStyles::wideNavigationButton());
    connect(m_btnBack, &QPushButton::clicked, this, &PvBoardDialog::onGoBack);
    btnLayout->addWidget(m_btnBack);

    m_btnForward = new QPushButton(QStringLiteral("1手進む ▶"), this);
    m_btnForward->setMinimumWidth(100);
    m_btnForward->setToolTip(tr("1手進む"));
    m_btnForward->setStyleSheet(ButtonStyles::wideNavigationButton());
    connect(m_btnForward, &QPushButton::clicked, this, &PvBoardDialog::onGoForward);
    btnLayout->addWidget(m_btnForward);

    m_btnLast = new QPushButton(QStringLiteral("最後 ⏭"), this);
    m_btnLast->setMinimumWidth(80);
    m_btnLast->setToolTip(tr("最後の局面まで進む"));
    m_btnLast->setStyleSheet(ButtonStyles::wideNavigationButton());
    connect(m_btnLast, &QPushButton::clicked, this, &PvBoardDialog::onGoLast);
    btnLayout->addWidget(m_btnLast);

    mainLayout->addLayout(btnLayout);

    // 閉じるボタン
    QPushButton* closeBtn = new QPushButton(tr("閉じる"), this);
    closeBtn->setMinimumWidth(100);
    closeBtn->setStyleSheet(ButtonStyles::secondaryNeutral());
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    QHBoxLayout* closeBtnLayout = new QHBoxLayout();
    closeBtnLayout->addStretch();
    closeBtnLayout->addWidget(closeBtn);
    closeBtnLayout->addStretch();
    mainLayout->addLayout(closeBtnLayout);
}

void PvBoardDialog::updateButtonStates()
{
    m_btnFirst->setEnabled(m_controller->canGoBack());
    m_btnBack->setEnabled(m_controller->canGoBack());
    m_btnForward->setEnabled(m_controller->canGoForward());
    m_btnLast->setEnabled(m_controller->canGoForward());
}

void PvBoardDialog::updateBoardDisplay()
{
    const QString sfen = m_controller->currentSfen();
    if (sfen.isEmpty()) return;

    // 現在の局面SFENを取得して盤面を更新
    m_board->setSfen(sfen);
    m_shogiView->update();

    // ハイライトを更新
    updateMoveHighlights();

    // 手番表示を更新
    const bool isBlackTurn = m_controller->isBlackTurn();
    m_shogiView->setActiveSide(isBlackTurn);
    m_shogiView->updateTurnIndicator(isBlackTurn ? ShogiGameController::Player1
                                                  : ShogiGameController::Player2);

    // 手数ラベルを更新
    const int ply = m_controller->currentPly();
    const int totalMoves = m_controller->totalMoves();
    QString plyText = tr("手数: %1 / %2").arg(ply).arg(totalMoves);
    if (ply == 0) {
        plyText += tr(" (開始局面)");
    } else {
        const QString moveText = m_controller->currentMoveText();
        if (!moveText.isEmpty()) {
            plyText += QStringLiteral(" [%1]").arg(moveText);
        }
    }
    m_plyLabel->setText(plyText);
}

void PvBoardDialog::onGoFirst()
{
    m_controller->goFirst();
    updateBoardDisplay();
    updateButtonStates();
}

void PvBoardDialog::onGoBack()
{
    if (m_controller->goBack()) {
        updateBoardDisplay();
        updateButtonStates();
    }
}

void PvBoardDialog::onGoForward()
{
    if (m_controller->goForward()) {
        updateBoardDisplay();
        updateButtonStates();
    }
}

void PvBoardDialog::onGoLast()
{
    m_controller->goLast();
    updateBoardDisplay();
    updateButtonStates();
}

void PvBoardDialog::onEnlargeBoard()
{
    if (m_shogiView) {
        m_shogiView->enlargeBoard(false);
        hideClockLabels();
        adjustSize();
    }
}

void PvBoardDialog::onReduceBoard()
{
    if (m_shogiView) {
        m_shogiView->reduceBoard(false);
        hideClockLabels();
        adjustSize();
    }
}

void PvBoardDialog::setFlipMode(bool flipped)
{
    if (!m_shogiView) return;

    m_shogiView->setFlipMode(flipped);
    if (flipped) {
        m_shogiView->setPiecesFlip();
    } else {
        m_shogiView->setPieces();
    }
    m_shogiView->update();
}

void PvBoardDialog::onFlipBoard()
{
    if (!m_shogiView) return;

    const bool flipped = !m_shogiView->getFlipMode();
    m_shogiView->setFlipMode(flipped);
    if (flipped) {
        m_shogiView->setPiecesFlip();
    } else {
        m_shogiView->setPieces();
    }
    m_shogiView->update();
}

bool PvBoardDialog::eventFilter(QObject* watched, QEvent* event)
{
    // ShogiViewのCtrl+ホイールイベントを横取りして処理
    if (watched == m_shogiView && event->type() == QEvent::Wheel) {
        QWheelEvent* wheelEvent = static_cast<QWheelEvent*>(event);
        if (wheelEvent->modifiers() & Qt::ControlModifier) {
            const int delta = wheelEvent->angleDelta().y();
            if (delta > 0) {
                onEnlargeBoard();
            } else if (delta < 0) {
                onReduceBoard();
            }
            return true;
        }
    }
    return QDialog::eventFilter(watched, event);
}

void PvBoardDialog::hideClockLabels()
{
    if (m_shogiView) {
        m_shogiView->setClockEnabled(false);
    }
}

void PvBoardDialog::clearMoveHighlights()
{
    if (!m_shogiView) return;

    if (m_fromHighlight) {
        m_shogiView->removeHighlight(m_fromHighlight.get());
        m_fromHighlight.reset();
    }
    if (m_toHighlight) {
        m_shogiView->removeHighlight(m_toHighlight.get());
        m_toHighlight.reset();
    }
}

void PvBoardDialog::updateMoveHighlights()
{
    clearMoveHighlights();

    if (!m_shogiView) return;

    const PvHighlightCoords coords = m_controller->computeHighlightCoords();
    if (!coords.valid) return;

    if (coords.hasFrom) {
        m_fromHighlight = std::make_unique<ShogiView::FieldHighlight>(
            coords.fromFile, coords.fromRank, QColor(255, 0, 0, 50));
        m_shogiView->addHighlight(m_fromHighlight.get());
    }
    if (coords.hasTo) {
        m_toHighlight = std::make_unique<ShogiView::FieldHighlight>(
            coords.toFile, coords.toRank, QColor(255, 255, 0));
        m_shogiView->addHighlight(m_toHighlight.get());
    }

    m_shogiView->update();
}

void PvBoardDialog::saveWindowSize()
{
    DialogUtils::saveDialogSize(this, AnalysisSettings::setPvBoardDialogSize);
}

void PvBoardDialog::adjustWindowToContents()
{
    if (m_shogiView) {
        m_shogiView->updateBoardSize();
        hideClockLabels();
    }
    adjustSize();
}

void PvBoardDialog::closeEvent(QCloseEvent* event)
{
    saveWindowSize();
    QDialog::closeEvent(event);
}
