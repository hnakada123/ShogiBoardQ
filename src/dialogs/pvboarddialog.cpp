/// @file pvboarddialog.cpp
/// @brief 読み筋盤面ダイアログクラスの実装

#include "pvboarddialog.h"
#include "applicationfonts.h"
#include "boardviewerwidget.h"
#include "kifupresentation.h"
#include "pvline.h"
#include "buttonstyles.h"
#include "shogiview.h"
#include "shogiviewhighlighting.h"
#include "analysissettings.h"
#include "appsettings.h"
#include "dialogfontscale.h"

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
    , m_line(std::make_unique<PvLine>(baseSfen, pvMoves))
{
    setWindowTitle(tr("読み筋表示"));
    setMinimumSize(kMinimumSize);

    buildUi();
    // ウィンドウは開くたびに盤に合わせる（fitToBoard）ので、ウィンドウの大きさは保存しない
    DialogFontScale::install(this, AppSettings::pvBoardDialogFontSize);

    // 読み筋の各手数の局面を盤に渡す（0 が開始局面）。表示する手数はビューアの番号だけで持つ
    m_viewer->setSfenList(m_line->positions());

    // ShogiViewの初期サイズを確定し、時計を隠す
    m_viewer->fitToBoard();
    updatePlyDisplay();

    // レイアウト確定後に自動調整（起動直後のサイズ不足を防ぐ）
    QTimer::singleShot(0, m_viewer, &BoardViewerWidget::fitToBoard);
}

PvBoardDialog::~PvBoardDialog() = default;

void PvBoardDialog::setKanjiPv(const QString& kanjiPv)
{
    m_kanjiPv = kanjiPv;
    m_pvLabel->setText(m_line->displayPv(m_kanjiPv));
    m_pvLabel->setToolTip(m_line->displayPv(m_kanjiPv, true));
    updatePlyDisplay();
}

void PvBoardDialog::setPlayerNames(const QString& blackName, const QString& whiteName)
{
    m_viewer->view()->chrome()->card(Turn::Black)->setName(blackName.isEmpty() ? tr("先手") : blackName);
    m_viewer->view()->chrome()->card(Turn::White)->setName(whiteName.isEmpty() ? tr("後手") : whiteName);
}

void PvBoardDialog::setLastMove(const QString& lastMove)
{
    m_lastMove = lastMove;
    m_line->setArrival(m_lastMove, m_prevSfen);
    updatePlyDisplay();
}

void PvBoardDialog::setPrevSfenForHighlight(const QString& prevSfen)
{
    m_prevSfen = prevSfen;
    m_line->setArrival(m_lastMove, m_prevSfen);
    updatePlyDisplay();
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

void PvBoardDialog::onPlyChanged(int)
{
    updatePlyDisplay();
}

void PvBoardDialog::updatePlyDisplay()
{
    const int ply = qMax(0, m_viewer->currentIndex());

    // 直前の手のハイライト（開始局面では、その局面に至った手）
    const PvLine::Highlight highlight = m_line->highlight(ply);
    ShogiViewHighlighting* highlighting = m_viewer->view()->highlighting();
    highlighting->setHighlights(BoardHighlightLayer::LastMoveFrom,
                                highlight.from ? QList<QPoint>{*highlight.from} : QList<QPoint>{});
    highlighting->setHighlights(BoardHighlightLayer::LastMoveTo,
                                highlight.to ? QList<QPoint>{*highlight.to} : QList<QPoint>{});

    QString plyText = tr("手数: %1 / %2").arg(ply).arg(m_line->moveCount());
    if (ply == 0) {
        plyText += tr(" (開始局面)");
    } else {
        const QString moveText = m_line->moveText(ply);
        if (!moveText.isEmpty()) plyText += QStringLiteral(" [%1]").arg(moveText);
    }
    m_viewer->setPositionText(plyText);
}

