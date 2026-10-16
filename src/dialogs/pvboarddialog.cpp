/// @file pvboarddialog.cpp
/// @brief 読み筋盤面ダイアログクラスの実装

#include "pvboarddialog.h"
#include "applicationfonts.h"
#include "boardviewerwidget.h"
#include "kifupresentation.h"
#include "pvboardcontroller.h"
#include "buttonstyles.h"
#include "shogiview.h"
#include "shogiviewhighlighting.h"
#include "analysissettings.h"
#include "dialogutils.h"
#include "appsettings.h"
#include "dialogfontscale.h"
#include "logcategories.h"

#include <QCloseEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

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

    buildUi();
    DialogFontScale::install(this, AppSettings::pvBoardDialogFontSize);

    // 読み筋の各手数の局面を盤に渡す（0 が開始局面）
    QStringList positions;
    m_controller->goFirst();
    positions.append(m_controller->currentSfen());
    while (m_controller->goForward()) positions.append(m_controller->currentSfen());
    m_controller->goFirst();
    m_viewer->setSfenList(positions);

    // ShogiViewの初期サイズを確定し、時計を隠す
    m_viewer->fitToBoard();
    updatePlyDisplay();

    // レイアウト確定後に自動調整（起動直後のサイズ不足を防ぐ）
    QTimer::singleShot(0, m_viewer, &BoardViewerWidget::fitToBoard);
}

PvBoardDialog::~PvBoardDialog() = default;

void PvBoardDialog::setKanjiPv(const QString& kanjiPv)
{
    m_controller->setKanjiPv(kanjiPv);
    m_pvLabel->setText(m_controller->displayPv());
    m_pvLabel->setToolTip(m_controller->displayPv(true));
    updatePlyDisplay();
}

void PvBoardDialog::setPlayerNames(const QString& blackName, const QString& whiteName)
{
    m_viewer->view()->chrome()->setBlackPlayerName(blackName.isEmpty() ? tr("先手") : blackName);
    m_viewer->view()->chrome()->setWhitePlayerName(whiteName.isEmpty() ? tr("後手") : whiteName);
}

void PvBoardDialog::setLastMove(const QString& lastMove)
{
    m_controller->setLastMove(lastMove);
    qCDebug(lcUi) << "setLastMove: lastMove=" << lastMove << " currentPly=" << m_controller->currentPly();
    if (m_controller->currentPly() == 0) updateMoveHighlights();
}

void PvBoardDialog::setPrevSfenForHighlight(const QString& prevSfen)
{
    m_controller->setPrevSfen(prevSfen);
    qCDebug(lcUi) << "setPrevSfenForHighlight: prevSfen=" << prevSfen.left(60);
    if (m_controller->currentPly() == 0) updateMoveHighlights();
}

void PvBoardDialog::setFlipMode(bool flipped)
{
    m_viewer->setFlipped(flipped);
}

void PvBoardDialog::buildUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(8);

    // 読み筋ラベル
    m_pvLabel = new QLabel(this);
    ApplicationFonts::useJapaneseFont(m_pvLabel,
        KifuPresentation::options().notation == KifuPresentation::Notation::Japanese);
    m_pvLabel->setWordWrap(true);
    m_pvLabel->setStyleSheet(QStringLiteral(
        "QLabel { background-color: #f5f5f5; border: 1px solid #ccc; padding: 8px; }"));
    m_pvLabel->setMinimumHeight(60);
    mainLayout->addWidget(m_pvLabel);

    // 盤・拡大縮小・回転・局面送り
    m_viewer = new BoardViewerWidget(BoardViewerWidget::ZoomToolbar | BoardViewerWidget::Navigation
                                         | BoardViewerWidget::ScrollArea | BoardViewerWidget::FitWindowOnZoom,
                                     BoardViewerWidget::ButtonLabels::Symbols, nullptr, this);
    m_viewer->setNavigationToolTips(tr("最初の局面に戻る"), tr("1手戻る"), tr("1手進む"), tr("最後の局面まで進む"));
    connect(m_viewer, &BoardViewerWidget::currentIndexChanged, this, &PvBoardDialog::onPlyChanged);
    mainLayout->addWidget(m_viewer, 1);

    // 閉じるボタン
    auto* closeBtn = new QPushButton(tr("閉じる"), this);
    closeBtn->setMinimumWidth(100);
    closeBtn->setStyleSheet(ButtonStyles::secondaryNeutral());
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    auto* closeBtnLayout = new QHBoxLayout();
    closeBtnLayout->addStretch();
    closeBtnLayout->addWidget(closeBtn);
    mainLayout->addLayout(closeBtnLayout);
}

void PvBoardDialog::onPlyChanged(int ply)
{
    m_controller->goFirst();
    for (int i = 0; i < ply && m_controller->goForward(); ++i) {}
    updatePlyDisplay();
}

void PvBoardDialog::updatePlyDisplay()
{
    updateMoveHighlights();
    const int ply = m_controller->currentPly();
    QString plyText = tr("手数: %1 / %2").arg(ply).arg(m_controller->totalMoves());
    if (ply == 0) {
        plyText += tr(" (開始局面)");
    } else {
        const QString moveText = m_controller->currentMoveText();
        if (!moveText.isEmpty()) plyText += QStringLiteral(" [%1]").arg(moveText);
    }
    m_viewer->setPositionText(plyText);
}

void PvBoardDialog::updateMoveHighlights()
{
    ShogiViewHighlighting* highlighting = m_viewer->view()->highlighting();
    const PvHighlightCoords coords = m_controller->computeHighlightCoords();
    const bool from = coords.valid && coords.hasFrom;
    const bool to = coords.valid && coords.hasTo;
    highlighting->setHighlights(BoardHighlightLayer::LastMoveFrom,
                                from ? QList<QPoint>{QPoint(coords.fromFile, coords.fromRank)} : QList<QPoint>{});
    highlighting->setHighlights(BoardHighlightLayer::LastMoveTo,
                                to ? QList<QPoint>{QPoint(coords.toFile, coords.toRank)} : QList<QPoint>{});
}

void PvBoardDialog::closeEvent(QCloseEvent* event)
{
    DialogUtils::saveDialogSize(this, AnalysisSettings::pvBoardDialogSize);
    QDialog::closeEvent(event);
}
