/// @file josekimovedialog.cpp
/// @brief 定跡手入力ダイアログクラスの実装

#include "josekimovedialog.h"
#include "buttonstyles.h"
#include "settingsservice.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QRegularExpression>
#include <QStackedWidget>

// 筋の表示用（1-9）
static const QStringList kFileLabels = {
    QStringLiteral("１"), QStringLiteral("２"), QStringLiteral("３"),
    QStringLiteral("４"), QStringLiteral("５"), QStringLiteral("６"),
    QStringLiteral("７"), QStringLiteral("８"), QStringLiteral("９")
};

// 段の表示用（一〜九）
static const QStringList kRankLabels = {
    QStringLiteral("一"), QStringLiteral("二"), QStringLiteral("三"),
    QStringLiteral("四"), QStringLiteral("五"), QStringLiteral("六"),
    QStringLiteral("七"), QStringLiteral("八"), QStringLiteral("九")
};

// 駒種の表示用
static const QStringList kPieceLabels = {
    QStringLiteral("歩"), QStringLiteral("香"), QStringLiteral("桂"),
    QStringLiteral("銀"), QStringLiteral("金"), QStringLiteral("角"),
    QStringLiteral("飛")
};

// 駒種のUSI形式
static const QStringList kPieceUsi = {
    QStringLiteral("P"), QStringLiteral("L"), QStringLiteral("N"),
    QStringLiteral("S"), QStringLiteral("G"), QStringLiteral("B"),
    QStringLiteral("R")
};

JosekiMoveDialog::JosekiMoveDialog(QWidget *parent, bool isEdit)
    : QDialog(parent)
    , m_fontSize(SettingsService::josekiMoveDialogFontSize())
{
    setupUi(isEdit);

    // ウィンドウサイズを復元
    QSize savedSize = SettingsService::josekiMoveDialogSize();
    if (savedSize.isValid() && savedSize.width() > 100 && savedSize.height() > 100) {
        resize(savedSize);
    }
}

void JosekiMoveDialog::setupUi(bool isEdit)
{
    setWindowTitle(isEdit ? tr("定跡手の編集") : tr("定跡手の追加"));
    setMinimumWidth(500);
    
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    
    // === フォントサイズ変更ボタン（左上に配置） ===
    QHBoxLayout *fontLayout = new QHBoxLayout();
    
    m_fontDecreaseBtn = new QPushButton(tr("A-"), this);
    m_fontDecreaseBtn->setToolTip(tr("フォントサイズを縮小"));
    m_fontDecreaseBtn->setFixedWidth(36);
    m_fontDecreaseBtn->setStyleSheet(ButtonStyles::fontButton());
    fontLayout->addWidget(m_fontDecreaseBtn);

    m_fontIncreaseBtn = new QPushButton(tr("A+"), this);
    m_fontIncreaseBtn->setToolTip(tr("フォントサイズを拡大"));
    m_fontIncreaseBtn->setFixedWidth(36);
    m_fontIncreaseBtn->setStyleSheet(ButtonStyles::fontButton());
    fontLayout->addWidget(m_fontIncreaseBtn);
    
    fontLayout->addStretch();
    mainLayout->addLayout(fontLayout);
    
    // === 指し手入力グループ ===（編集モードでは非表示）
    QGroupBox *moveGroup = createMoveInputWidget(this, false);
    if (isEdit) {
        moveGroup->hide();
    }
    mainLayout->addWidget(moveGroup);
    
    // === 予想応手入力グループ ===（編集モードでは非表示）
    QGroupBox *nextMoveGroup = createMoveInputWidget(this, true);
    if (isEdit) {
        nextMoveGroup->hide();
    }
    mainLayout->addWidget(nextMoveGroup);
    
    // === 編集対象の定跡手表示 ===（編集モードのみ）
    m_editMoveLabel = new QLabel(this);
    m_editMoveLabel->setStyleSheet(QStringLiteral(
        "QLabel {"
        "  font-size: 14pt;"
        "  font-weight: bold;"
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
    valueLabel->setFixedWidth(80);
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
    depthLabel->setFixedWidth(80);
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
    frequencyLabel->setFixedWidth(80);
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
    
    // エラー表示
    m_moveErrorLabel = new QLabel(this);
    m_moveErrorLabel->setStyleSheet(QStringLiteral("color: red; font-weight: bold;"));
    m_moveErrorLabel->hide();
    bottomLayout->addWidget(m_moveErrorLabel);
    
    bottomLayout->addStretch();
    mainLayout->addLayout(bottomLayout);
    
    // === ボタン ===
    m_buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        Qt::Horizontal, this);
    m_buttonBox->button(QDialogButtonBox::Ok)->setText(isEdit ? tr("更新") : tr("追加"));
    m_buttonBox->button(QDialogButtonBox::Cancel)->setText(tr("キャンセル"));
    mainLayout->addWidget(m_buttonBox);
    
    // シグナル・スロット接続
    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &JosekiMoveDialog::validateInput);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_fontIncreaseBtn, &QPushButton::clicked, this, &JosekiMoveDialog::onFontSizeIncrease);
    connect(m_fontDecreaseBtn, &QPushButton::clicked, this, &JosekiMoveDialog::onFontSizeDecrease);
    
    // 初期状態を設定
    onMoveInputModeChanged();
    onNextMoveInputModeChanged();
    updateMovePreview();
    updateNextMovePreview();
    applyFontSize();
}

QGroupBox* JosekiMoveDialog::createMoveInputWidget(QWidget *parent, bool isNextMove)
{
    QString title = isNextMove ? tr("予想応手") : tr("指し手");
    QGroupBox *group = new QGroupBox(title, parent);
    QVBoxLayout *layout = new QVBoxLayout(group);
    
    // ラジオボタン
    QHBoxLayout *radioLayout = new QHBoxLayout();
    
    QRadioButton *boardRadio = new QRadioButton(tr("盤上の駒を動かす"), group);
    QRadioButton *dropRadio = new QRadioButton(tr("持ち駒を打つ"), group);
    
    QButtonGroup *typeGroup = new QButtonGroup(group);
    typeGroup->addButton(boardRadio, 0);
    typeGroup->addButton(dropRadio, 1);
    
    radioLayout->addWidget(boardRadio);
    radioLayout->addWidget(dropRadio);
    
    if (isNextMove) {
        QRadioButton *noneRadio = new QRadioButton(tr("なし"), group);
        typeGroup->addButton(noneRadio, 2);
        radioLayout->addWidget(noneRadio);
        m_nextMoveNoneRadio = noneRadio;
        noneRadio->setChecked(true);
    } else {
        boardRadio->setChecked(true);
    }
    
    radioLayout->addStretch();
    layout->addLayout(radioLayout);
    
    // 入力エリアコンテナ
    QWidget *inputWidget = new QWidget(group);
    QVBoxLayout *inputLayout = new QVBoxLayout(inputWidget);
    inputLayout->setContentsMargins(0, 0, 0, 0);
    
    // 盤上移動用ウィジェット
    QWidget *boardWidget = new QWidget(inputWidget);
    QHBoxLayout *boardLayout = new QHBoxLayout(boardWidget);
    boardLayout->setContentsMargins(0, 0, 0, 0);
    
    QLabel *fromLabel = new QLabel(tr("移動元:"), boardWidget);
    QComboBox *fromFileCombo = new QComboBox(boardWidget);
    QComboBox *fromRankCombo = new QComboBox(boardWidget);
    
    // 筋のコンボボックス設定
    for (int i = 0; i < 9; ++i) {
        fromFileCombo->addItem(kFileLabels[i], i + 1);
    }
    // 段のコンボボックス設定
    for (int i = 0; i < 9; ++i) {
        fromRankCombo->addItem(kRankLabels[i], QChar('a' + i));
    }
    
    QLabel *toLabel = new QLabel(tr("移動先:"), boardWidget);
    QComboBox *toFileCombo = new QComboBox(boardWidget);
    QComboBox *toRankCombo = new QComboBox(boardWidget);
    
    for (int i = 0; i < 9; ++i) {
        toFileCombo->addItem(kFileLabels[i], i + 1);
    }
    for (int i = 0; i < 9; ++i) {
        toRankCombo->addItem(kRankLabels[i], QChar('a' + i));
    }
    
    QLabel *promoteLabel = new QLabel(tr("成り:"), boardWidget);
    QComboBox *promoteCombo = new QComboBox(boardWidget);
    promoteCombo->addItem(tr("不成"), 0);
    promoteCombo->addItem(tr("成"), 1);
    
    boardLayout->addWidget(fromLabel);
    boardLayout->addWidget(fromFileCombo);
    boardLayout->addWidget(fromRankCombo);
    boardLayout->addSpacing(10);
    boardLayout->addWidget(toLabel);
    boardLayout->addWidget(toFileCombo);
    boardLayout->addWidget(toRankCombo);
    boardLayout->addSpacing(10);
    boardLayout->addWidget(promoteLabel);
    boardLayout->addWidget(promoteCombo);
    boardLayout->addStretch();
    
    inputLayout->addWidget(boardWidget);
    
    // 駒打ち用ウィジェット
    QWidget *dropWidget = new QWidget(inputWidget);
    QHBoxLayout *dropLayout = new QHBoxLayout(dropWidget);
    dropLayout->setContentsMargins(0, 0, 0, 0);
    
    QLabel *pieceLabel = new QLabel(tr("駒種:"), dropWidget);
    QComboBox *pieceCombo = new QComboBox(dropWidget);
    for (int i = 0; i < kPieceLabels.size(); ++i) {
        pieceCombo->addItem(kPieceLabels[i], kPieceUsi[i]);
    }
    
    QLabel *dropToLabel = new QLabel(tr("打ち先:"), dropWidget);
    QComboBox *dropToFileCombo = new QComboBox(dropWidget);
    QComboBox *dropToRankCombo = new QComboBox(dropWidget);
    
    for (int i = 0; i < 9; ++i) {
        dropToFileCombo->addItem(kFileLabels[i], i + 1);
    }
    for (int i = 0; i < 9; ++i) {
        dropToRankCombo->addItem(kRankLabels[i], QChar('a' + i));
    }
    
    dropLayout->addWidget(pieceLabel);
    dropLayout->addWidget(pieceCombo);
    dropLayout->addSpacing(10);
    dropLayout->addWidget(dropToLabel);
    dropLayout->addWidget(dropToFileCombo);
    dropLayout->addWidget(dropToRankCombo);
    dropLayout->addStretch();
    
    inputLayout->addWidget(dropWidget);
    dropWidget->hide();  // 初期状態では非表示
    
    layout->addWidget(inputWidget);
    
    // プレビュー行
    QHBoxLayout *previewLayout = new QHBoxLayout();
    QLabel *previewTitleLabel = new QLabel(tr("プレビュー:"), group);
    QLabel *previewLabel = new QLabel(group);
    previewLabel->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 12pt; color: #333;"));
    
    QLabel *usiTitleLabel = new QLabel(tr("USI形式:"), group);
    QLabel *usiLabel = new QLabel(group);
    usiLabel->setStyleSheet(QStringLiteral("font-family: monospace; color: #666;"));
    
    previewLayout->addWidget(previewTitleLabel);
    previewLayout->addWidget(previewLabel);
    previewLayout->addSpacing(20);
    previewLayout->addWidget(usiTitleLabel);
    previewLayout->addWidget(usiLabel);
    previewLayout->addStretch();
    
    layout->addLayout(previewLayout);
    
    // メンバ変数に保存
    if (isNextMove) {
        m_nextMoveBoardRadio = boardRadio;
        m_nextMoveDropRadio = dropRadio;
        m_nextMoveTypeGroup = typeGroup;
        m_nextMoveFromFileCombo = fromFileCombo;
        m_nextMoveFromRankCombo = fromRankCombo;
        m_nextMoveToFileCombo = toFileCombo;
        m_nextMoveToRankCombo = toRankCombo;
        m_nextMoveDropPieceCombo = pieceCombo;
        m_nextMoveDropToFileCombo = dropToFileCombo;
        m_nextMoveDropToRankCombo = dropToRankCombo;
        m_nextMovePromoteCombo = promoteCombo;
        m_nextMovePreviewLabel = previewLabel;
        m_nextMoveUsiLabel = usiLabel;
        m_nextMoveBoardWidget = boardWidget;
        m_nextMoveDropWidget = dropWidget;
        m_nextMoveInputWidget = inputWidget;
        
        connect(typeGroup, &QButtonGroup::idClicked, this, &JosekiMoveDialog::onNextMoveInputModeChanged);
        connect(fromFileCombo, &QComboBox::currentIndexChanged, this, &JosekiMoveDialog::onNextMoveComboChanged);
        connect(fromRankCombo, &QComboBox::currentIndexChanged, this, &JosekiMoveDialog::onNextMoveComboChanged);
        connect(toFileCombo, &QComboBox::currentIndexChanged, this, &JosekiMoveDialog::onNextMoveComboChanged);
        connect(toRankCombo, &QComboBox::currentIndexChanged, this, &JosekiMoveDialog::onNextMoveComboChanged);
        connect(pieceCombo, &QComboBox::currentIndexChanged, this, &JosekiMoveDialog::onNextMoveComboChanged);
        connect(promoteCombo, &QComboBox::currentIndexChanged, this, &JosekiMoveDialog::onNextMoveComboChanged);
        connect(dropToFileCombo, &QComboBox::currentIndexChanged, this, &JosekiMoveDialog::onNextMoveComboChanged);
        connect(dropToRankCombo, &QComboBox::currentIndexChanged, this, &JosekiMoveDialog::onNextMoveComboChanged);
    } else {
        m_moveBoardRadio = boardRadio;
        m_moveDropRadio = dropRadio;
        m_moveTypeGroup = typeGroup;
        m_moveFromFileCombo = fromFileCombo;
        m_moveFromRankCombo = fromRankCombo;
        m_moveToFileCombo = toFileCombo;
        m_moveToRankCombo = toRankCombo;
        m_moveDropPieceCombo = pieceCombo;
        m_moveDropToFileCombo = dropToFileCombo;
        m_moveDropToRankCombo = dropToRankCombo;
        m_movePromoteCombo = promoteCombo;
        m_movePreviewLabel = previewLabel;
        m_moveUsiLabel = usiLabel;
        m_moveBoardWidget = boardWidget;
        m_moveDropWidget = dropWidget;
        
        connect(typeGroup, &QButtonGroup::idClicked, this, &JosekiMoveDialog::onMoveInputModeChanged);
        connect(fromFileCombo, &QComboBox::currentIndexChanged, this, &JosekiMoveDialog::onMoveComboChanged);
        connect(fromRankCombo, &QComboBox::currentIndexChanged, this, &JosekiMoveDialog::onMoveComboChanged);
        connect(toFileCombo, &QComboBox::currentIndexChanged, this, &JosekiMoveDialog::onMoveComboChanged);
        connect(toRankCombo, &QComboBox::currentIndexChanged, this, &JosekiMoveDialog::onMoveComboChanged);
        connect(pieceCombo, &QComboBox::currentIndexChanged, this, &JosekiMoveDialog::onMoveComboChanged);
        connect(promoteCombo, &QComboBox::currentIndexChanged, this, &JosekiMoveDialog::onMoveComboChanged);
        connect(dropToFileCombo, &QComboBox::currentIndexChanged, this, &JosekiMoveDialog::onMoveComboChanged);
        connect(dropToRankCombo, &QComboBox::currentIndexChanged, this, &JosekiMoveDialog::onMoveComboChanged);
    }
    
    return group;
}

void JosekiMoveDialog::onMoveInputModeChanged()
{
    int id = m_moveTypeGroup->checkedId();
    bool isDrop = (id == 1);
    
    m_moveBoardWidget->setVisible(!isDrop);
    m_moveDropWidget->setVisible(isDrop);
    
    updateMovePreview();
}

void JosekiMoveDialog::onMoveComboChanged()
{
    updateMovePreview();
}

void JosekiMoveDialog::onNextMoveInputModeChanged()
{
    int id = m_nextMoveTypeGroup->checkedId();
    bool isNone = (id == 2);
    bool isDrop = (id == 1);
    
    m_nextMoveInputWidget->setVisible(!isNone);
    
    if (!isNone) {
        m_nextMoveBoardWidget->setVisible(!isDrop);
        m_nextMoveDropWidget->setVisible(isDrop);
    }
    
    updateNextMovePreview();
}

void JosekiMoveDialog::onNextMoveComboChanged()
{
    updateNextMovePreview();
}

QString JosekiMoveDialog::generateUsiMove(bool isDrop,
                                           QComboBox *pieceCombo,
                                           QComboBox *fromFileCombo,
                                           QComboBox *fromRankCombo,
                                           QComboBox *toFileCombo,
                                           QComboBox *toRankCombo,
                                           QComboBox *promoteCombo) const
{
    if (isDrop) {
        // 駒打ち: P*5e形式
        QString piece = pieceCombo->currentData().toString();
        int toFile = toFileCombo->currentData().toInt();
        QChar toRank = toRankCombo->currentData().toChar();
        
        return piece + QStringLiteral("*") + QString::number(toFile) + toRank;
    } else {
        // 盤上移動: 7g7f形式
        int fromFile = fromFileCombo->currentData().toInt();
        QChar fromRank = fromRankCombo->currentData().toChar();
        int toFile = toFileCombo->currentData().toInt();
        QChar toRank = toRankCombo->currentData().toChar();
        bool promote = promoteCombo->currentData().toInt() == 1;
        
        QString result = QString::number(fromFile) + fromRank + 
                         QString::number(toFile) + toRank;
        if (promote) {
            result += QStringLiteral("+");
        }
        return result;
    }
}

QString JosekiMoveDialog::usiToJapanese(const QString &usiMove) const
{
    if (usiMove.isEmpty() || usiMove == QStringLiteral("none")) {
        return tr("なし");
    }
    
    // 全角数字
    static const QString zenkakuDigits = QStringLiteral(" １２３４５６７８９");
    // 漢数字
    static const QString kanjiRanks = QStringLiteral(" 一二三四五六七八九");
    
    // 駒打ちパターン: P*5e
    if (usiMove.size() >= 4 && usiMove.at(1) == QChar('*')) {
        QChar pieceChar = usiMove.at(0);
        int toFile = usiMove.at(2).toLatin1() - '0';
        int toRank = usiMove.at(3).toLatin1() - 'a' + 1;
        
        QString pieceKanji;
        switch (pieceChar.toLatin1()) {
        case 'P': pieceKanji = QStringLiteral("歩"); break;
        case 'L': pieceKanji = QStringLiteral("香"); break;
        case 'N': pieceKanji = QStringLiteral("桂"); break;
        case 'S': pieceKanji = QStringLiteral("銀"); break;
        case 'G': pieceKanji = QStringLiteral("金"); break;
        case 'B': pieceKanji = QStringLiteral("角"); break;
        case 'R': pieceKanji = QStringLiteral("飛"); break;
        default: pieceKanji = QString(pieceChar); break;
        }
        
        QString result;
        if (toFile >= 1 && toFile <= 9) result += zenkakuDigits.at(toFile);
        if (toRank >= 1 && toRank <= 9) result += kanjiRanks.at(toRank);
        result += pieceKanji + QStringLiteral("打");
        
        return result;
    }
    
    // 通常移動パターン: 7g7f, 7g7f+
    if (usiMove.size() >= 4) {
        int fromFile = usiMove.at(0).toLatin1() - '0';
        int fromRank = usiMove.at(1).toLatin1() - 'a' + 1;
        int toFile = usiMove.at(2).toLatin1() - '0';
        int toRank = usiMove.at(3).toLatin1() - 'a' + 1;
        bool promotes = (usiMove.size() >= 5 && usiMove.at(4) == QChar('+'));
        
        QString result;
        if (toFile >= 1 && toFile <= 9) result += zenkakuDigits.at(toFile);
        if (toRank >= 1 && toRank <= 9) result += kanjiRanks.at(toRank);
        
        // 移動元の情報
        result += QStringLiteral("(") + QString::number(fromFile) + QString::number(fromRank) + QStringLiteral(")");
        
        if (promotes) {
            result += QStringLiteral("成");
        }
        
        return result;
    }
    
    return usiMove;
}

void JosekiMoveDialog::updateMovePreview()
{
    int id = m_moveTypeGroup->checkedId();
    bool isDrop = (id == 1);
    
    // 駒打ち用のコンボボックスを取得（メンバ変数を使用）
    QComboBox *toFileCombo = isDrop ? m_moveDropToFileCombo : m_moveToFileCombo;
    QComboBox *toRankCombo = isDrop ? m_moveDropToRankCombo : m_moveToRankCombo;
    
    QString usi = generateUsiMove(isDrop, 
                                   m_moveDropPieceCombo,
                                   m_moveFromFileCombo, 
                                   m_moveFromRankCombo,
                                   toFileCombo,
                                   toRankCombo,
                                   m_movePromoteCombo);
    
    m_moveUsiLabel->setText(usi);
    m_movePreviewLabel->setText(usiToJapanese(usi));
}

void JosekiMoveDialog::updateNextMovePreview()
{
    int id = m_nextMoveTypeGroup->checkedId();
    
    if (id == 2) {
        // なし
        m_nextMoveUsiLabel->setText(QStringLiteral("none"));
        m_nextMovePreviewLabel->setText(tr("なし"));
        return;
    }
    
    bool isDrop = (id == 1);
    
    // 駒打ち用のコンボボックスを取得（メンバ変数を使用）
    QComboBox *toFileCombo = isDrop ? m_nextMoveDropToFileCombo : m_nextMoveToFileCombo;
    QComboBox *toRankCombo = isDrop ? m_nextMoveDropToRankCombo : m_nextMoveToRankCombo;
    
    QString usi = generateUsiMove(isDrop,
                                   m_nextMoveDropPieceCombo,
                                   m_nextMoveFromFileCombo,
                                   m_nextMoveFromRankCombo,
                                   toFileCombo,
                                   toRankCombo,
                                   m_nextMovePromoteCombo);
    
    m_nextMoveUsiLabel->setText(usi);
    m_nextMovePreviewLabel->setText(usiToJapanese(usi));
}

bool JosekiMoveDialog::isValidUsiMove(const QString &move) const
{
    if (move.isEmpty()) {
        return false;
    }
    
    // 駒打ち形式: X*YZ (例: P*5e)
    static QRegularExpression dropPattern(QStringLiteral("^[PLNSGBR]\\*[1-9][a-i]$"));
    if (dropPattern.match(move).hasMatch()) {
        return true;
    }
    
    // 通常移動形式: XYZW または XYZW+ (例: 7g7f, 8h2b+)
    static QRegularExpression movePattern(QStringLiteral("^[1-9][a-i][1-9][a-i]\\+?$"));
    if (movePattern.match(move).hasMatch()) {
        return true;
    }
    
    return false;
}

void JosekiMoveDialog::validateInput()
{
    m_moveErrorLabel->hide();

    QString moveStr = move();

    if (moveStr.isEmpty() || !isValidUsiMove(moveStr)) {
        m_moveErrorLabel->setText(tr("指し手が正しく設定されていません"));
        m_moveErrorLabel->show();
        return;
    }

    SettingsService::setJosekiMoveDialogSize(size());
    accept();
}

QString JosekiMoveDialog::move() const
{
    int id = m_moveTypeGroup->checkedId();
    bool isDrop = (id == 1);

    // 駒打ち用のコンボボックスを取得（メンバ変数を直接使用）
    QComboBox *toFileCombo = isDrop ? m_moveDropToFileCombo : m_moveToFileCombo;
    QComboBox *toRankCombo = isDrop ? m_moveDropToRankCombo : m_moveToRankCombo;

    // nullチェック
    if (!toFileCombo || !toRankCombo) {
        qWarning() << "JosekiMoveDialog::move() - コンボボックス取得失敗";
        return QString();
    }

    return generateUsiMove(isDrop,
                           m_moveDropPieceCombo,
                           m_moveFromFileCombo,
                           m_moveFromRankCombo,
                           toFileCombo,
                           toRankCombo,
                           m_movePromoteCombo);
}

void JosekiMoveDialog::setMove(const QString &move)
{
    setComboFromUsi(move, false);
}

QString JosekiMoveDialog::nextMove() const
{
    int id = m_nextMoveTypeGroup->checkedId();

    if (id == 2) {
        return QStringLiteral("none");
    }

    bool isDrop = (id == 1);

    // 駒打ち用のコンボボックスを取得（メンバ変数を直接使用）
    QComboBox *toFileCombo = isDrop ? m_nextMoveDropToFileCombo : m_nextMoveToFileCombo;
    QComboBox *toRankCombo = isDrop ? m_nextMoveDropToRankCombo : m_nextMoveToRankCombo;

    // nullチェック
    if (!toFileCombo || !toRankCombo) {
        qWarning() << "JosekiMoveDialog::nextMove() - コンボボックス取得失敗";
        return QStringLiteral("none");
    }

    return generateUsiMove(isDrop,
                           m_nextMoveDropPieceCombo,
                           m_nextMoveFromFileCombo,
                           m_nextMoveFromRankCombo,
                           toFileCombo,
                           toRankCombo,
                           m_nextMovePromoteCombo);
}

void JosekiMoveDialog::setNextMove(const QString &nextMove)
{
    if (nextMove == QStringLiteral("none") || nextMove.isEmpty()) {
        m_nextMoveNoneRadio->setChecked(true);
        onNextMoveInputModeChanged();
        return;
    }
    
    setComboFromUsi(nextMove, true);
}

void JosekiMoveDialog::setComboFromUsi(const QString &usiMove, bool isNextMove)
{
    if (usiMove.isEmpty()) return;
    
    // 参照するウィジェットを選択
    QRadioButton *boardRadio = isNextMove ? m_nextMoveBoardRadio : m_moveBoardRadio;
    QRadioButton *dropRadio = isNextMove ? m_nextMoveDropRadio : m_moveDropRadio;
    QComboBox *fromFileCombo = isNextMove ? m_nextMoveFromFileCombo : m_moveFromFileCombo;
    QComboBox *fromRankCombo = isNextMove ? m_nextMoveFromRankCombo : m_moveFromRankCombo;
    QComboBox *toFileCombo = isNextMove ? m_nextMoveToFileCombo : m_moveToFileCombo;
    QComboBox *toRankCombo = isNextMove ? m_nextMoveToRankCombo : m_moveToRankCombo;
    QComboBox *pieceCombo = isNextMove ? m_nextMoveDropPieceCombo : m_moveDropPieceCombo;
    QComboBox *promoteCombo = isNextMove ? m_nextMovePromoteCombo : m_movePromoteCombo;
    
    // 駒打ちパターン: P*5e
    if (usiMove.size() >= 4 && usiMove.at(1) == QChar('*')) {
        dropRadio->setChecked(true);
        
        QChar piece = usiMove.at(0);
        int toFile = usiMove.at(2).toLatin1() - '0';
        int toRank = usiMove.at(3).toLatin1() - 'a';
        
        // 駒種を設定
        qsizetype pieceIdx = kPieceUsi.indexOf(QString(piece));
        if (pieceIdx >= 0) {
            pieceCombo->setCurrentIndex(static_cast<int>(pieceIdx));
        }
        
        // 打ち先のコンボボックスを取得して設定（メンバ変数を直接使用）
        QComboBox *dropToFileCombo = isNextMove ? m_nextMoveDropToFileCombo : m_moveDropToFileCombo;
        QComboBox *dropToRankCombo = isNextMove ? m_nextMoveDropToRankCombo : m_moveDropToRankCombo;
        if (dropToFileCombo && dropToRankCombo) {
            dropToFileCombo->setCurrentIndex(toFile - 1);
            dropToRankCombo->setCurrentIndex(toRank);
        }
        
        if (isNextMove) {
            onNextMoveInputModeChanged();
        } else {
            onMoveInputModeChanged();
        }
        return;
    }
    
    // 通常移動パターン: 7g7f, 7g7f+
    if (usiMove.size() >= 4) {
        boardRadio->setChecked(true);
        
        int fromFile = usiMove.at(0).toLatin1() - '0';
        int fromRank = usiMove.at(1).toLatin1() - 'a';
        int toFile = usiMove.at(2).toLatin1() - '0';
        int toRank = usiMove.at(3).toLatin1() - 'a';
        bool promotes = (usiMove.size() >= 5 && usiMove.at(4) == QChar('+'));
        
        fromFileCombo->setCurrentIndex(fromFile - 1);
        fromRankCombo->setCurrentIndex(fromRank);
        toFileCombo->setCurrentIndex(toFile - 1);
        toRankCombo->setCurrentIndex(toRank);
        promoteCombo->setCurrentIndex(promotes ? 1 : 0);
        
        if (isNextMove) {
            onNextMoveInputModeChanged();
        } else {
            onMoveInputModeChanged();
        }
    }
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
        SettingsService::setJosekiMoveDialogFontSize(m_fontSize);
    }
}

void JosekiMoveDialog::onFontSizeDecrease()
{
    if (m_fontSize > 8) {
        m_fontSize--;
        applyFontSize();
        SettingsService::setJosekiMoveDialogFontSize(m_fontSize);
    }
}

void JosekiMoveDialog::applyFontSize()
{
    QFont font = this->font();
    font.setPointSize(m_fontSize);
    
    // ダイアログ全体に適用
    setFont(font);
    
    // プレビューラベルは少し大きめに
    QFont previewFont = font;
    previewFont.setPointSize(m_fontSize + 2);
    previewFont.setBold(true);
    m_movePreviewLabel->setFont(previewFont);
    m_nextMovePreviewLabel->setFont(previewFont);
    
    // 編集対象の定跡手ラベルも更新
    if (m_editMoveLabel && m_editMoveLabel->isVisible()) {
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
