/// @file pvboarddialog.cpp
/// @brief 読み筋盤面ダイアログクラスの実装

#include "pvboarddialog.h"
#include "applicationfonts.h"
#include "plaintooltip.h"
#include "boardviewerwidget.h"
#include "kifupresentation.h"
#include "pvline.h"
#include "pvmovestrip.h"
#include "buttonstyles.h"
#include "shogiview.h"
#include "shogiviewhighlighting.h"
#include "appsettings.h"
#include "dialogfontscale.h"

#include <QHBoxLayout>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

namespace {
constexpr QSize kMinimumSize{400, 500};
} // namespace

PvBoardDialog::PvBoardDialog(const PvText& pv, QWidget* parent)
    : QDialog(parent)
    , m_line(std::make_unique<PvLine>(pv))
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

void PvBoardDialog::setPlayerNames(const QString& blackName, const QString& whiteName)
{
    m_viewer->view()->chrome()->card(Turn::Black)->setName(blackName);
    m_viewer->view()->chrome()->card(Turn::White)->setName(whiteName);
}

void PvBoardDialog::setLastMove(const QString& lastMove)
{
    m_line->setArrival(lastMove);
    updatePlyDisplay();
}

void PvBoardDialog::setFlipMode(bool flipped)
{
    m_viewer->setFlipped(flipped);
}

void PvBoardDialog::setSquareSize(int squareSize)
{
    m_viewer->view()->setSquareSize(squareSize);
    m_viewer->fitToBoard();   // ウィンドウを盤に合わせ直す
}

void PvBoardDialog::buildUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(8);

    // 読み筋の手の帯（先頭は開始局面）
    QStringList labels{tr("開始局面")};
    for (int ply = 1; ply <= m_line->moveCount(); ++ply) labels.append(m_line->moveText(ply));
    m_moveStrip = new PvMoveStrip(labels, m_line->ending(), this);
    ApplicationFonts::useJapaneseFont(m_moveStrip,
        KifuPresentation::options().notation == KifuPresentation::Notation::Japanese);
    m_moveStrip->setToolTip(plainToolTip(m_line->displayPv(true)));
    mainLayout->addWidget(m_moveStrip);

    // 盤・拡大縮小・回転・局面送り
    m_viewer = new BoardViewerWidget(BoardViewerWidget::ZoomToolbar | BoardViewerWidget::Navigation
                                         | BoardViewerWidget::ScrollArea | BoardViewerWidget::FitWindowOnZoom,
                                     BoardViewerWidget::ButtonLabels::Symbols, nullptr, this);
    m_viewer->setNavigationToolTips(tr("最初の局面に戻る"), tr("1手戻る"), tr("1手進む"), tr("最後の局面まで進む"));
    connect(m_viewer, &BoardViewerWidget::currentIndexChanged, this, &PvBoardDialog::onPlyChanged);
    connect(m_moveStrip, &PvMoveStrip::positionRequested, m_viewer, &BoardViewerWidget::setCurrentIndex);
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
    m_moveStrip->setCurrent(ply);

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

