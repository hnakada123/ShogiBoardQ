/// @file josekimovedialog.cpp
/// @brief 定跡手入力ダイアログクラスの実装

#include "josekimovedialog.h"
#include "dialogfontscale.h"
#include "elidelabel.h"
#include "josekimoveboard.h"
#include "josekimoveinputwidget.h"
#include "josekisettings.h"
#include "enginemovevalidator.h"
#include "shogiposition.h"
#include "sfenreplay.h"

#include <QDialogButtonBox>
#include <QGroupBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include <limits>

JosekiMoveDialog::JosekiMoveDialog(QWidget *parent, bool isEdit)
    : QDialog(parent)
    , m_isEdit(isEdit)
{
    setupUi(isEdit);
}

void JosekiMoveDialog::setupUi(bool isEdit)
{
    setWindowTitle(isEdit ? tr("定跡手の編集") : tr("定跡手の追加"));

    auto *mainLayout = new QHBoxLayout(this);
    mainLayout->setSpacing(16);

    // === 盤と案内 ===（案内は1行に収め、文が変わっても盤を動かさない）
    auto *boardLayout = new QVBoxLayout();
    boardLayout->setSpacing(6);
    m_guideLabel = new ElideLabel(this);
    m_guideLabel->setObjectName(QStringLiteral("moveGuide"));
    m_guideLabel->setElideMode(Qt::ElideRight);
    QFont guideFont = m_guideLabel->font();
    guideFont.setBold(true);
    m_guideLabel->setFont(guideFont);
    boardLayout->addWidget(m_guideLabel);
    m_board = new JosekiMoveBoard(this);
    boardLayout->addWidget(m_board, 1);
    mainLayout->addLayout(boardLayout);

    // === 入力欄 ===
    auto *formLayout = new QVBoxLayout();
    formLayout->setSpacing(10);
    m_positionLabel = new QLabel(this);
    m_positionLabel->setWordWrap(true);
    m_positionLabel->hide();
    formLayout->addWidget(m_positionLabel);

    // === 指し手入力ウィジェット ===（編集モードでは非表示）
    m_moveInput = new JosekiMoveInputWidget(QStringLiteral("moveInput"), tr("① 指し手"), false, this);
    if (isEdit) {
        m_moveInput->hide();
    }
    formLayout->addWidget(m_moveInput);

    // === 予想応手入力ウィジェット ===
    m_nextMoveInput = new JosekiMoveInputWidget(QStringLiteral("nextMoveInput"), tr("② 予想応手（省略可）"), true, this);

    // === 編集対象の定跡手表示 ===（編集モードのみ）
    m_editMoveLabel = new QLabel(this);
    m_editMoveLabel->setStyleSheet(QStringLiteral(
        "QLabel {"
        "  color: #333;"
        "  padding: 8px;"
        "  background-color: #f0f0f0;"
        "  border: 1px solid #ccc;"
        "  border-radius: 4px;"
        "}"
    ));
    m_editMoveLabel->setTextFormat(Qt::PlainText);
    m_editMoveLabel->setAlignment(Qt::AlignCenter);
    QFont editFont = m_editMoveLabel->font();
    editFont.setPointSize(editFont.pointSize() + 4);
    editFont.setBold(true);
    m_editMoveLabel->setFont(editFont);
    if (isEdit) {
        formLayout->addWidget(m_editMoveLabel);
    } else {
        m_editMoveLabel->hide();
    }

    formLayout->addWidget(m_nextMoveInput);

    // === 評価情報グループ ===（項目名を上にして3つを横に並べる）
    QGroupBox *evalGroup = new QGroupBox(tr("評価情報"), this);
    evalGroup->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    auto *evalLayout = new QGridLayout(evalGroup);
    evalLayout->setVerticalSpacing(2);
    const auto addEvalField = [this, evalLayout](int column, const QString &title, QSpinBox *spinBox) {
        auto *label = new QLabel(title, this);
        label->setBuddy(spinBox);
        evalLayout->addWidget(label, 0, column);
        evalLayout->addWidget(spinBox, 1, column);
    };

    // 評価値
    m_valueSpinBox = new QSpinBox(this);
    m_valueSpinBox->setRange(std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
    m_valueSpinBox->setValue(0);
    m_valueSpinBox->setToolTip(tr("この指し手を指した時の評価値。\n"
                                   "正の値は手番側が有利、負の値は手番側が不利。"));
    addEvalField(0, tr("評価値:"), m_valueSpinBox);

    // 深さ
    m_depthSpinBox = new QSpinBox(this);
    m_depthSpinBox->setRange(0, std::numeric_limits<int>::max());
    m_depthSpinBox->setValue(32);
    m_depthSpinBox->setToolTip(tr("この評価値を算出した時の探索深さ。"));
    addEvalField(1, tr("探索深さ:"), m_depthSpinBox);

    // 出現頻度
    m_frequencySpinBox = new QSpinBox(this);
    m_frequencySpinBox->setRange(0, std::numeric_limits<int>::max());
    m_frequencySpinBox->setValue(1);
    m_frequencySpinBox->setToolTip(tr("この指し手が選択された回数。"));
    addEvalField(2, tr("出現頻度:"), m_frequencySpinBox);

    formLayout->addWidget(evalGroup);

    // === コメントグループ ===
    QGroupBox *commentGroup = new QGroupBox(tr("コメント（任意）"), this);
    commentGroup->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    QVBoxLayout *commentLayout = new QVBoxLayout(commentGroup);

    m_commentEdit = new QLineEdit(this);
    m_commentEdit->setObjectName(QStringLiteral("commentEdit"));
    m_commentEdit->setPlaceholderText(tr("コメントを入力（任意）"));
    commentLayout->addWidget(m_commentEdit);
    formLayout->addWidget(commentGroup);

    // === エラー表示行 ===
    m_moveErrorLabel = new QLabel(this);
    m_moveErrorLabel->setStyleSheet(QStringLiteral("color: red; font-weight: bold;"));
    m_moveErrorLabel->setWordWrap(true);
    m_moveErrorLabel->setObjectName(QStringLiteral("moveError"));
    m_moveErrorLabel->hide();
    formLayout->addWidget(m_moveErrorLabel);

    formLayout->addStretch();

    // === ボタン ===
    QHBoxLayout *buttonLayout = new QHBoxLayout();

    m_fontDecreaseBtn = new QPushButton(this);
    buttonLayout->addWidget(m_fontDecreaseBtn);
    m_fontIncreaseBtn = new QPushButton(this);
    buttonLayout->addWidget(m_fontIncreaseBtn);

    buttonLayout->addStretch();

    m_buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        Qt::Horizontal, this);
    m_buttonBox->button(QDialogButtonBox::Ok)->setText(isEdit ? tr("更新") : tr("追加"));
    m_buttonBox->button(QDialogButtonBox::Cancel)->setText(tr("キャンセル"));
    buttonLayout->addWidget(m_buttonBox);

    formLayout->addLayout(buttonLayout);
    mainLayout->addLayout(formLayout, 1);

    // シグナル・スロット接続
    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &JosekiMoveDialog::validateInput);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    connect(m_moveInput, &JosekiMoveInputWidget::moveChanged, this, &JosekiMoveDialog::updateValidation);
    connect(m_moveInput, &JosekiMoveInputWidget::retried, this, &JosekiMoveDialog::clearNextMove);
    connect(m_nextMoveInput, &JosekiMoveInputWidget::moveChanged, this, &JosekiMoveDialog::updateValidation);
    connect(m_board, &JosekiMoveBoard::movePlayed, this, &JosekiMoveDialog::onBoardMovePlayed);

    // 文字の大きさ（編集する定跡手は本文より 4pt 大きい太字のまま拡大・縮小する）
    DialogFontScale::Options fontOptions;
    fontOptions.maxSize = 18;
    fontOptions.decrease = m_fontDecreaseBtn;
    fontOptions.increase = m_fontIncreaseBtn;
    fontOptions.size = &JosekiSettings::josekiMoveDialogSize;
    DialogFontScale::install(this, JosekiSettings::josekiMoveDialogFontSize, fontOptions);
    updateValidation();
    // 開いたらキーボードで一覧を選べるようにする（盤の操作列のボタンを先に作っているので明示する）
    (isEdit ? m_nextMoveInput : m_moveInput)->setFocus();
}

namespace {
bool isLegalMove(const QString &sfen, const QString &usi)
{
    const auto move = UsiMove::parse(usi);
    const std::optional<Position> board = Position::fromSfen(sfen);
    if (!move || !board) return false;
    return EngineMoveValidator::isLegal(EngineMoveValidator::load(*board), *move);
}
} // namespace

QString JosekiMoveDialog::move() const
{
    return m_isEdit ? m_editMove : m_moveInput->usiMove();
}

QString JosekiMoveDialog::inputError() const
{
    // 追加では合法手からしか選べない。編集で渡された指し手は局面に合わないことがある（ファイルの手）
    if (move().isEmpty()) return tr("指し手を選んでください。");
    if (!m_currentSfen.isEmpty() && !isLegalMove(m_currentSfen, move()))
        return tr("指し手が現在の局面の合法手ではありません。移動元・移動先・成りを確認してください。");
    if (m_nextMoveInput->usiMove().isEmpty()) return tr("予想応手を選ぶか、「なし」を選んでください。");
    return {};
}

void JosekiMoveDialog::updateValidation()
{
    const QString error = inputError();
    // 指し手をまだ選んでいないのは誤りではない（次にすることは盤の上の案内で示す）
    const bool showError = !error.isEmpty() && !move().isEmpty();
    m_moveErrorLabel->setText(showError ? error : QString());
    m_moveErrorLabel->setVisible(showError);
    m_buttonBox->button(QDialogButtonBox::Ok)->setEnabled(error.isEmpty());
    // 予想応手は着手後の局面の合法手から選ぶ
    const QString after = (!m_currentSfen.isEmpty() && isLegalMove(m_currentSfen, move()))
        ? SfenReplay::positionAfter(m_currentSfen, move()) : QString();
    m_nextMoveInput->setCurrentSfen(after);
    m_nextMoveInput->setEnabled(!after.isEmpty());
    updateBoard(after);
}

void JosekiMoveDialog::updateBoard(const QString &after)
{
    const QString next = m_nextMoveInput->usiMove();
    const bool hasNextMove = !next.isEmpty() && next != JosekiMoveInputWidget::kNoMove;
    const bool choosingMove = !m_isEdit && after.isEmpty();
    const bool choosingNextMove = !after.isEmpty() && !hasNextMove;
    m_moveInput->setBoardInputTarget(choosingMove);
    m_nextMoveInput->setBoardInputTarget(choosingNextMove);

    if (choosingMove) {
        m_guideLabel->setFullText(tr("指し手を盤で指すか、一覧から選んでください"));
        m_board->showPosition(m_currentSfen, QString(), true);
    } else if (choosingNextMove) {
        m_guideLabel->setFullText(tr("予想応手を盤で指すか、一覧から選べます（省略できます）"));
        m_board->showPosition(after, move(), true);
    } else if (hasNextMove) {
        m_guideLabel->setFullText(tr("指し手と予想応手が決まりました"));
        m_board->showPosition(SfenReplay::positionAfter(after, next), next, false);
    } else {
        // 編集する指し手が局面に合わない（定跡ファイルの手）。誤りの文は入力欄の下に出す
        m_guideLabel->setFullText(QString());
        m_board->showPosition(m_currentSfen, QString(), false);
    }
}

void JosekiMoveDialog::onBoardMovePlayed(const QString &usiMove)
{
    if (move().isEmpty()) m_moveInput->setUsiMove(usiMove);
    else m_nextMoveInput->setUsiMove(usiMove);
}

void JosekiMoveDialog::clearNextMove()
{
    m_nextMoveInput->setUsiMove(JosekiMoveInputWidget::kNoMove);
}

void JosekiMoveDialog::validateInput()
{
    updateValidation();
    if (inputError().isEmpty()) accept();
}

void JosekiMoveDialog::done(int result)
{
    if (result == QDialog::Accepted && !inputError().isEmpty()) {
        updateValidation();
        return;
    }
    QDialog::done(result);
}

void JosekiMoveDialog::setCurrentSfen(const QString &sfen)
{
    m_currentSfen = sfen;
    m_moveInput->setCurrentSfen(sfen);
    const std::optional<Position> board = sfen.isEmpty() ? std::nullopt : Position::fromSfen(sfen);
    m_positionLabel->setVisible(board.has_value());
    if (board)
        m_positionLabel->setText(tr("登録する局面: %1番").arg(board->sideToMove() == Turn::Black ? tr("先手") : tr("後手")));
    updateValidation();
}

void JosekiMoveDialog::setEntry(const JosekiMove &entry)
{
    if (m_isEdit) m_editMove = entry.move;
    m_moveInput->setUsiMove(entry.move);
    // 予想応手は指し手の後の局面で選び直す（updateValidation がその局面を渡す）
    m_nextMoveInput->setUsiMove(entry.nextMove.value_or(JosekiMoveInputWidget::kNoMove));
    m_valueSpinBox->setValue(entry.value);
    m_depthSpinBox->setValue(entry.depth);
    m_frequencySpinBox->setValue(entry.frequency);
    m_commentEdit->setText(entry.comment);
    updateValidation();
}

JosekiMove JosekiMoveDialog::entry() const
{
    JosekiMove result;
    result.move = move();
    const QString next = m_nextMoveInput->usiMove();
    if (!next.isEmpty() && next != JosekiMoveInputWidget::kNoMove) result.nextMove = next;
    result.value = m_valueSpinBox->value();
    result.depth = m_depthSpinBox->value();
    result.frequency = m_frequencySpinBox->value();
    result.comment = m_commentEdit->text().trimmed();
    return result;
}

void JosekiMoveDialog::setEditMoveDisplay(const QString &moveText)
{
    if (m_editMoveLabel) {
        m_editMoveLabel->setText(tr("定跡手: %1").arg(moveText));
    }
}
