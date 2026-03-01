/// @file josekimovedialog.cpp
/// @brief 定跡手入力ダイアログクラスの実装

#include "josekimovedialog.h"
#include "buttonstyles.h"
#include "dialogutils.h"
#include "josekimoveinputwidget.h"
#include "josekisettings.h"

#include <QDialogButtonBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

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

    // === 指し手入力ウィジェット ===（編集モードでは非表示）
    m_moveInput = new JosekiMoveInputWidget(tr("指し手"), false, this);
    if (isEdit) {
        m_moveInput->hide();
    }
    mainLayout->addWidget(m_moveInput);

    // === 予想応手入力ウィジェット ===（編集モードでは非表示）
    m_nextMoveInput = new JosekiMoveInputWidget(tr("予想応手"), true, this);
    if (isEdit) {
        m_nextMoveInput->hide();
    }
    mainLayout->addWidget(m_nextMoveInput);

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
    m_editMoveLabel->setAlignment(Qt::AlignCenter);
    if (isEdit) {
        mainLayout->addWidget(m_editMoveLabel);
    } else {
        m_editMoveLabel->hide();
    }

    // === 評価情報グループ ===
    QGroupBox *evalGroup = new QGroupBox(tr("評価情報"), this);
    QVBoxLayout *evalMainLayout = new QVBoxLayout(evalGroup);

    // 評価値
    QHBoxLayout *valueLayout = new QHBoxLayout();
    QLabel *valueLabel = new QLabel(tr("評価値:"), this);
    m_valueSpinBox = new QSpinBox(this);
    m_valueSpinBox->setRange(-99999, 99999);
    m_valueSpinBox->setValue(0);
    m_valueSpinBox->setToolTip(tr("この指し手を指した時の評価値。\n"
                                   "正の値は手番側が有利、負の値は手番側が不利。"));
    valueLayout->addWidget(valueLabel);
    valueLayout->addWidget(m_valueSpinBox);
    valueLayout->addStretch();
    evalMainLayout->addLayout(valueLayout);

    // 深さ
    QHBoxLayout *depthLayout = new QHBoxLayout();
    QLabel *depthLabel = new QLabel(tr("探索深さ:"), this);
    m_depthSpinBox = new QSpinBox(this);
    m_depthSpinBox->setRange(0, 999);
    m_depthSpinBox->setValue(32);
    m_depthSpinBox->setToolTip(tr("この評価値を算出した時の探索深さ。"));
    depthLayout->addWidget(depthLabel);
    depthLayout->addWidget(m_depthSpinBox);
    depthLayout->addStretch();
    evalMainLayout->addLayout(depthLayout);

    // 出現頻度
    QHBoxLayout *frequencyLayout = new QHBoxLayout();
    QLabel *frequencyLabel = new QLabel(tr("出現頻度:"), this);
    m_frequencySpinBox = new QSpinBox(this);
    m_frequencySpinBox->setRange(0, 999999);
    m_frequencySpinBox->setValue(1);
    m_frequencySpinBox->setToolTip(tr("この指し手が選択された回数。"));
    frequencyLayout->addWidget(frequencyLabel);
    frequencyLayout->addWidget(m_frequencySpinBox);
    frequencyLayout->addStretch();
    evalMainLayout->addLayout(frequencyLayout);

    mainLayout->addWidget(evalGroup);

    // === コメントグループ ===
    QGroupBox *commentGroup = new QGroupBox(tr("コメント（任意）"), this);
    QVBoxLayout *commentLayout = new QVBoxLayout(commentGroup);

    m_commentEdit = new QLineEdit(this);
    m_commentEdit->setPlaceholderText(tr("コメントを入力（任意）"));
    commentLayout->addWidget(m_commentEdit);
    mainLayout->addWidget(commentGroup);

    // === エラー表示行 ===
    QHBoxLayout *bottomLayout = new QHBoxLayout();

    m_moveErrorLabel = new QLabel(this);
    m_moveErrorLabel->setStyleSheet(QStringLiteral("color: red; font-weight: bold;"));
    m_moveErrorLabel->hide();
    bottomLayout->addWidget(m_moveErrorLabel);

    bottomLayout->addStretch();
    mainLayout->addLayout(bottomLayout);

    // === ボタン ===
    QHBoxLayout *buttonLayout = new QHBoxLayout();

    m_fontDecreaseBtn = new QPushButton(tr("A-"), this);
    m_fontDecreaseBtn->setToolTip(tr("フォントサイズを縮小"));
    m_fontDecreaseBtn->setFixedWidth(36);
    m_fontDecreaseBtn->setStyleSheet(ButtonStyles::fontButton());
    buttonLayout->addWidget(m_fontDecreaseBtn);

    m_fontIncreaseBtn = new QPushButton(tr("A+"), this);
    m_fontIncreaseBtn->setToolTip(tr("フォントサイズを拡大"));
    m_fontIncreaseBtn->setFixedWidth(36);
    m_fontIncreaseBtn->setStyleSheet(ButtonStyles::fontButton());
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

    applyFontSize();
}

void JosekiMoveDialog::validateInput()
{
    m_moveErrorLabel->hide();

    QString moveStr = move();

    if (moveStr.isEmpty() || !JosekiMoveInputWidget::isValidUsiMove(moveStr)) {
        m_moveErrorLabel->setText(tr("指し手が正しく設定されていません"));
        m_moveErrorLabel->show();
        return;
    }

    DialogUtils::saveDialogSize(this, JosekiSettings::setJosekiMoveDialogSize);
    accept();
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

void JosekiMoveDialog::setNextMove(const QString &nextMove)
{
    m_nextMoveInput->setUsiMove(nextMove);
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
