/// @file josekimoveinputwidget.cpp
/// @brief 定跡手入力ウィジェットクラスの実装

#include "josekimoveinputwidget.h"

#include "enginemovevalidator.h"
#include "josekitablemodel.h"
#include "shogiposition.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QPalette>
#include <QPushButton>
#include <QSignalBlocker>

#include <algorithm>

namespace {

struct LegalMove {
    UsiMove move;
    Piece moving = Piece::None;
    QString text;
};

/// 局面の合法手を、駒の種類・移動先・移動元の順に並べて表記を付ける。表記は定跡ウィンドウの表・
/// ダイアログの「定跡手: …」と同じ定跡手の表記（JosekiTableModel::moveText。表記の設定に合わせる）
QList<LegalMove> legalMovesOf(const QString &sfen)
{
    const std::optional<Position> board = Position::fromSfen(sfen);
    if (!board) return {};
    const Turn mover = board->sideToMove();
    QList<LegalMove> result;
    const QList<UsiMove> moves = EngineMoveValidator::legalMoves(*board, mover);
    for (const UsiMove &move : moves) {
        const Piece moving = move.isDrop() ? move.dropPieceFor(mover) : board->pieceAt(move.from);
        if (moving == Piece::None) continue;
        result.append({move, moving, JosekiTableModel::moveText(*board, move)});
    }
    std::sort(result.begin(), result.end(), [](const LegalMove &a, const LegalMove &b) {
        const auto key = [](const LegalMove &m) {
            return std::make_tuple(static_cast<int>(toBlack(demote(m.moving))), m.move.isDrop(), m.move.to.file,
                                   m.move.to.rank, m.move.from.file, m.move.from.rank, m.move.promote);
        };
        return key(a) < key(b);
    });
    return result;
}

} // namespace

JosekiMoveInputWidget::JosekiMoveInputWidget(const QString &name, const QString &title, bool hasNoneOption,
                                             QWidget *parent)
    : QGroupBox(title, parent)
    , m_hasNoneOption(hasNoneOption)
{
    // 予想応手は「なし」から始める
    if (hasNoneOption) m_requested = kNoMove;
    setObjectName(name);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    auto *layout = new QHBoxLayout(this);

    m_combo = new QComboBox(this);
    m_combo->setObjectName(name + QStringLiteral("Combo"));
    m_combo->setMaxVisibleItems(20);
    connect(m_combo, &QComboBox::currentIndexChanged, this, &JosekiMoveInputWidget::onCurrentIndexChanged);
    layout->addWidget(m_combo, 1);
    setFocusProxy(m_combo);

    m_retryButton = new QPushButton(tr("指し直す"), this);
    m_retryButton->setObjectName(name + QStringLiteral("Retry"));
    m_retryButton->setToolTip(tr("選んだ手を取り消す"));
    m_retryButton->setAutoDefault(false);
    connect(m_retryButton, &QPushButton::clicked, this, &JosekiMoveInputWidget::onRetryClicked);
    layout->addWidget(m_retryButton);

    populate();
}

QString JosekiMoveInputWidget::usiMove() const
{
    return m_combo->currentData().toString();
}

void JosekiMoveInputWidget::setUsiMove(const QString &usiMove)
{
    m_requested = usiMove;
    selectRequested();
}

void JosekiMoveInputWidget::setCurrentSfen(const QString &sfen)
{
    if (sfen == m_currentSfen) return;
    m_currentSfen = sfen;
    populate();
}

void JosekiMoveInputWidget::setBoardInputTarget(bool target)
{
    if (target == autoFillBackground()) return;
    setAutoFillBackground(target);
    if (!target) {
        setPalette(QPalette());
        return;
    }
    // 配色の種類（明暗）によらないよう、選択の色を薄くして敷く
    QColor tint = QPalette().color(QPalette::Highlight);
    tint.setAlpha(45);
    QPalette tinted;
    tinted.setColor(QPalette::Window, tint);
    setPalette(tinted);
}

void JosekiMoveInputWidget::populate()
{
    {
        const QSignalBlocker blocker(m_combo);
        m_combo->clear();
        if (m_hasNoneOption) m_combo->addItem(tr("なし"), kNoMove);
        if (!m_currentSfen.isEmpty()) {
            const QList<LegalMove> moves = legalMovesOf(m_currentSfen);
            for (const LegalMove &move : moves) m_combo->addItem(move.text, move.move.toString());
        }
        m_combo->setEnabled(m_combo->count() > 0);
        // 選んでいないときの表示。局面があるのに「局面がありません」と出さない
        m_combo->setPlaceholderText(m_combo->count() > 0 ? tr("合法手から選んでください") : tr("局面がありません"));
    }
    selectRequested();
}

void JosekiMoveInputWidget::selectRequested()
{
    const int index = m_requested.isEmpty() ? -1 : m_combo->findData(m_requested);
    {
        const QSignalBlocker blocker(m_combo);
        m_combo->setCurrentIndex(index);
    }
    onCurrentIndexChanged();
}

void JosekiMoveInputWidget::onCurrentIndexChanged()
{
    const QString move = usiMove();
    // 利用者が選び直したら、局面が変わってもその手を選ぶ
    if (!move.isEmpty()) m_requested = move;
    m_retryButton->setEnabled(!move.isEmpty() && move != kNoMove);
    emit moveChanged();
}

void JosekiMoveInputWidget::onRetryClicked()
{
    setUsiMove(m_hasNoneOption ? kNoMove : QString());
    emit retried();
}
