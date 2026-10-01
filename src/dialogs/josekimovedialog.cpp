/// @file josekimovedialog.cpp
/// @brief 定跡手入力ダイアログクラスの実装

#include "josekimovedialog.h"
#include "buttonstyles.h"
#include "dialogutils.h"
#include "josekimoveinputwidget.h"
#include "josekisettings.h"
#include "enginemovevalidator.h"
#include "shogiboard.h"
#include "sfenpositiontracer.h"

#include <QDialogButtonBox>
#include <QGroupBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include <limits>

JosekiMoveDialog::JosekiMoveDialog(QWidget *parent, bool isEdit)
    : QDialog(parent)
    , m_fontSize(JosekiSettings::josekiMoveDialogFontSize())
{
    setupUi(isEdit);

    // ウィンドウサイズを復元
    DialogUtils::restoreDialogSize(this, JosekiSettings::josekiMoveDialogSize());
}

void JosekiMoveDialog::setupUi(bool isEdit)
{
    setWindowTitle(isEdit ? tr("定跡手の編集") : tr("定跡手の追加"));
    setMinimumWidth(500);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);
    m_positionLabel = new QLabel(this);
    m_positionLabel->setWordWrap(true);
    m_positionLabel->hide();
    mainLayout->addWidget(m_positionLabel);

    // === 指し手入力ウィジェット ===（編集モードでは非表示）
    m_moveInput = new JosekiMoveInputWidget(tr("指し手"), false, this);
    if (isEdit) {
        m_moveInput->hide();
    }
    m_moveInput->setObjectName(QStringLiteral("moveInput"));
    mainLayout->addWidget(m_moveInput);

    // === 予想応手入力ウィジェット ===
    m_nextMoveInput = new JosekiMoveInputWidget(tr("予想応手"), true, this);
    m_nextMoveInput->setObjectName(QStringLiteral("nextMoveInput"));

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
    if (isEdit) {
        mainLayout->addWidget(m_editMoveLabel);
    } else {
        m_editMoveLabel->hide();
    }

    mainLayout->addWidget(m_nextMoveInput);

    // === 評価情報グループ ===
    QGroupBox *evalGroup = new QGroupBox(tr("評価情報"), this);
    evalGroup->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    QFormLayout *evalMainLayout = new QFormLayout(evalGroup);
    evalMainLayout->setFieldGrowthPolicy(QFormLayout::FieldsStayAtSizeHint);

    // 評価値
    m_valueSpinBox = new QSpinBox(this);
    m_valueSpinBox->setRange(std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
    m_valueSpinBox->setValue(0);
    m_valueSpinBox->setToolTip(tr("この指し手を指した時の評価値。\n"
                                   "正の値は手番側が有利、負の値は手番側が不利。"));
    evalMainLayout->addRow(tr("評価値:"), m_valueSpinBox);

    // 深さ
    m_depthSpinBox = new QSpinBox(this);
    m_depthSpinBox->setRange(0, std::numeric_limits<int>::max());
    m_depthSpinBox->setValue(32);
    m_depthSpinBox->setToolTip(tr("この評価値を算出した時の探索深さ。"));
    evalMainLayout->addRow(tr("探索深さ:"), m_depthSpinBox);

    // 出現頻度
    m_frequencySpinBox = new QSpinBox(this);
    m_frequencySpinBox->setRange(0, std::numeric_limits<int>::max());
    m_frequencySpinBox->setValue(1);
    m_frequencySpinBox->setToolTip(tr("この指し手が選択された回数。"));
    evalMainLayout->addRow(tr("出現頻度:"), m_frequencySpinBox);

    mainLayout->addWidget(evalGroup);

    // === コメントグループ ===
    QGroupBox *commentGroup = new QGroupBox(tr("コメント（任意）"), this);
    commentGroup->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    QVBoxLayout *commentLayout = new QVBoxLayout(commentGroup);

    m_commentEdit = new QLineEdit(this);
    m_commentEdit->setPlaceholderText(tr("コメントを入力（任意）"));
    commentLayout->addWidget(m_commentEdit);
    mainLayout->addWidget(commentGroup);

    // === エラー表示行 ===
    QHBoxLayout *bottomLayout = new QHBoxLayout();

    m_moveErrorLabel = new QLabel(this);
    m_moveErrorLabel->setStyleSheet(QStringLiteral("color: red; font-weight: bold;"));
    m_moveErrorLabel->setWordWrap(true);
    m_moveErrorLabel->setObjectName(QStringLiteral("moveError"));
    m_moveErrorLabel->hide();
    bottomLayout->addWidget(m_moveErrorLabel);

    bottomLayout->addStretch();
    mainLayout->addLayout(bottomLayout);

    mainLayout->addStretch();

    // === ボタン ===
    QHBoxLayout *buttonLayout = new QHBoxLayout();

    m_fontDecreaseBtn = new QPushButton(tr("A-"), this);
    m_fontDecreaseBtn->setToolTip(tr("フォントサイズを縮小"));
    m_fontDecreaseBtn->setFixedWidth(36);
    m_fontDecreaseBtn->setAutoDefault(false);
    m_fontDecreaseBtn->setStyleSheet(ButtonStyles::panelToolButton());
    buttonLayout->addWidget(m_fontDecreaseBtn);

    m_fontIncreaseBtn = new QPushButton(tr("A+"), this);
    m_fontIncreaseBtn->setToolTip(tr("フォントサイズを拡大"));
    m_fontIncreaseBtn->setFixedWidth(36);
    m_fontIncreaseBtn->setAutoDefault(false);
    m_fontIncreaseBtn->setStyleSheet(ButtonStyles::panelToolButton());
    buttonLayout->addWidget(m_fontIncreaseBtn);

    buttonLayout->addStretch();

    m_buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        Qt::Horizontal, this);
    m_buttonBox->button(QDialogButtonBox::Ok)->setText(isEdit ? tr("更新") : tr("追加"));
    m_buttonBox->button(QDialogButtonBox::Cancel)->setText(tr("キャンセル"));
    buttonLayout->addWidget(m_buttonBox);

    mainLayout->addLayout(buttonLayout);

    // シグナル・スロット接続
    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &JosekiMoveDialog::validateInput);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_fontIncreaseBtn, &QPushButton::clicked, this, &JosekiMoveDialog::onFontSizeIncrease);
    connect(m_fontDecreaseBtn, &QPushButton::clicked, this, &JosekiMoveDialog::onFontSizeDecrease);

    connect(m_moveInput, &JosekiMoveInputWidget::moveChanged, this, &JosekiMoveDialog::updateValidation);
    connect(m_nextMoveInput, &JosekiMoveInputWidget::moveChanged, this, &JosekiMoveDialog::updateValidation);
    applyFontSize();
    updateValidation();
}

namespace {
bool isLegalMove(const QString &sfen, const QString &usi)
{
    if (!JosekiMoveInputWidget::isValidUsiMove(usi)) return false;
    SfenPositionTracer tracer;
    if (!tracer.setFromSfen(sfen)) return false;
    ShogiBoard board;
    board.setSfen(sfen);
    auto moves = SfenPositionTracer::buildGameMoves(sfen, {usi});
    if (moves.size() != 1) return false;
    EngineMoveValidator validator;
    const auto turn = tracer.blackToMove() ? EngineMoveValidator::BLACK : EngineMoveValidator::WHITE;
    const auto status = validator.isLegalMove(turn, board.boardData(), board.pieceStand(), moves.first());
    return usi.endsWith(QLatin1Char('+')) ? status.promotingMoveExists : status.nonPromotingMoveExists;
}
} // namespace

QString JosekiMoveDialog::inputError() const
{
    if (!JosekiMoveInputWidget::isValidUsiMove(move()))
        return tr("移動元と移動先を別のマスに設定してください。");
    if (!m_currentSfen.isEmpty() && !isLegalMove(m_currentSfen, move()))
        return tr("指し手が現在の局面の合法手ではありません。移動元・移動先・成りを確認してください。");
    if (nextMove() != QStringLiteral("none")) {
        if (!JosekiMoveInputWidget::isValidUsiMove(nextMove()))
            return tr("予想応手の移動元と移動先を別のマスに設定してください。");
        if (!m_currentSfen.isEmpty()) {
            SfenPositionTracer tracer;
            if (!tracer.setFromSfen(m_currentSfen) || !tracer.applyUsiMove(move())
                || !isLegalMove(tracer.toSfenString(), nextMove()))
                return tr("予想応手が着手後の局面の合法手ではありません。");
        }
    }
    return {};
}

void JosekiMoveDialog::updateValidation()
{
    const QString error = inputError();
    m_moveErrorLabel->setText(error);
    m_moveErrorLabel->setVisible(!error.isEmpty());
    m_buttonBox->button(QDialogButtonBox::Ok)->setEnabled(error.isEmpty());
    SfenPositionTracer tracer;
    const bool validMove = !m_currentSfen.isEmpty() && isLegalMove(m_currentSfen, move())
        && tracer.setFromSfen(m_currentSfen) && tracer.applyUsiMove(move());
    m_nextMoveInput->setCurrentSfen(validMove ? tracer.toSfenString() : QString());
    m_nextMoveInput->setEnabled(m_currentSfen.isEmpty() || validMove);
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
    DialogUtils::saveDialogSize(this, JosekiSettings::setJosekiMoveDialogSize);
    QDialog::done(result);
}

void JosekiMoveDialog::setCurrentSfen(const QString &sfen)
{
    m_currentSfen = sfen;
    m_moveInput->setCurrentSfen(sfen);
    SfenPositionTracer tracer;
    const bool valid = !sfen.isEmpty() && tracer.setFromSfen(sfen);
    m_positionLabel->setVisible(valid);
    if (valid) m_positionLabel->setText(tr("登録する局面: %1番").arg(tracer.blackToMove() ? tr("先手") : tr("後手")));
    updateValidation();
}

void JosekiMoveDialog::setNextMove(const QString &move)
{
    m_nextMoveInput->setUsiMove(move);
}

QString JosekiMoveDialog::move() const
{
    return m_moveInput->usiMove();
}

void JosekiMoveDialog::setMove(const QString &move)
{
    m_moveInput->setUsiMove(move);
}

QString JosekiMoveDialog::nextMove() const
{
    return m_nextMoveInput->usiMove();
}

int JosekiMoveDialog::value() const
{
    return m_valueSpinBox->value();
}

void JosekiMoveDialog::setValue(int value)
{
    m_valueSpinBox->setValue(value);
}

int JosekiMoveDialog::depth() const
{
    return m_depthSpinBox->value();
}

void JosekiMoveDialog::setDepth(int depth)
{
    m_depthSpinBox->setValue(depth);
}

int JosekiMoveDialog::frequency() const
{
    return m_frequencySpinBox->value();
}

void JosekiMoveDialog::setFrequency(int frequency)
{
    m_frequencySpinBox->setValue(frequency);
}

QString JosekiMoveDialog::comment() const
{
    return m_commentEdit->text().trimmed();
}

void JosekiMoveDialog::setComment(const QString &comment)
{
    m_commentEdit->setText(comment);
}

void JosekiMoveDialog::onFontSizeIncrease()
{
    if (m_fontSize < 18) {
        m_fontSize++;
        applyFontSize();
        JosekiSettings::setJosekiMoveDialogFontSize(m_fontSize);
    }
}

void JosekiMoveDialog::onFontSizeDecrease()
{
    if (m_fontSize > 8) {
        m_fontSize--;
        applyFontSize();
        JosekiSettings::setJosekiMoveDialogFontSize(m_fontSize);
    }
}

void JosekiMoveDialog::applyFontSize()
{
    QFont font = this->font();
    font.setPointSize(m_fontSize);

    DialogUtils::applyFontToAllChildren(this, font);

    // 入力ウィジェットのプレビューラベルに拡大フォントを適用
    m_moveInput->applyFont(font);
    m_nextMoveInput->applyFont(font);

    // 編集対象の定跡手ラベルも更新
    if (m_editMoveLabel) {
        QFont editFont = font;
        editFont.setPointSize(m_fontSize + 4);
        editFont.setBold(true);
        m_editMoveLabel->setFont(editFont);
    }
}

void JosekiMoveDialog::setEditMoveDisplay(const QString &japaneseMove)
{
    if (m_editMoveLabel) {
        m_editMoveLabel->setText(tr("定跡手: %1").arg(japaneseMove));
    }
}
