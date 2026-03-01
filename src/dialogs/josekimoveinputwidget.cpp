/// @file josekimoveinputwidget.cpp
/// @brief 定跡手入力ウィジェットクラスの実装

#include "josekimoveinputwidget.h"

#include <QButtonGroup>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QRadioButton>
#include <QRegularExpression>
#include <QVBoxLayout>

#include <algorithm>
#include <array>

// 筋の表示用（1-9）
static constexpr std::array<QStringView, 9> kFileLabels = {
    u"１", u"２", u"３", u"４", u"５", u"６", u"７", u"８", u"９"
};

// 段の表示用（一〜九）
static constexpr std::array<QStringView, 9> kRankLabels = {
    u"一", u"二", u"三", u"四", u"五", u"六", u"七", u"八", u"九"
};

// 駒種の表示用
static constexpr std::array<QStringView, 7> kPieceLabels = {
    u"歩", u"香", u"桂", u"銀", u"金", u"角", u"飛"
};

// 駒種のUSI形式
static constexpr std::array<QStringView, 7> kPieceUsi = {
    u"P", u"L", u"N", u"S", u"G", u"B", u"R"
};

JosekiMoveInputWidget::JosekiMoveInputWidget(const QString &title, bool hasNoneOption, QWidget *parent)
    : QGroupBox(title, parent)
{
    setupUi(hasNoneOption);
    onInputModeChanged();
    updatePreview();
}

void JosekiMoveInputWidget::setupUi(bool hasNoneOption)
{
    QVBoxLayout *layout = new QVBoxLayout(this);

    // ラジオボタン
    QHBoxLayout *radioLayout = new QHBoxLayout();

    m_boardRadio = new QRadioButton(tr("盤上の駒を動かす"), this);
    m_dropRadio = new QRadioButton(tr("持ち駒を打つ"), this);

    m_typeGroup = new QButtonGroup(this);
    m_typeGroup->addButton(m_boardRadio, 0);
    m_typeGroup->addButton(m_dropRadio, 1);

    radioLayout->addWidget(m_boardRadio);
    radioLayout->addWidget(m_dropRadio);

    if (hasNoneOption) {
        m_noneRadio = new QRadioButton(tr("なし"), this);
        m_typeGroup->addButton(m_noneRadio, 2);
        radioLayout->addWidget(m_noneRadio);
        m_noneRadio->setChecked(true);
    } else {
        m_boardRadio->setChecked(true);
    }

    radioLayout->addStretch();
    layout->addLayout(radioLayout);

    // 入力エリアコンテナ
    m_inputWidget = new QWidget(this);
    QVBoxLayout *inputLayout = new QVBoxLayout(m_inputWidget);
    inputLayout->setContentsMargins(0, 0, 0, 0);

    // 盤上移動用ウィジェット
    m_boardWidget = new QWidget(m_inputWidget);
    QHBoxLayout *boardLayout = new QHBoxLayout(m_boardWidget);
    boardLayout->setContentsMargins(0, 0, 0, 0);

    QLabel *fromLabel = new QLabel(tr("移動元:"), m_boardWidget);
    m_fromFileCombo = new QComboBox(m_boardWidget);
    m_fromRankCombo = new QComboBox(m_boardWidget);

    for (int i = 0; i < 9; ++i) {
        m_fromFileCombo->addItem(kFileLabels[static_cast<size_t>(i)].toString(), i + 1);
    }
    for (int i = 0; i < 9; ++i) {
        m_fromRankCombo->addItem(kRankLabels[static_cast<size_t>(i)].toString(), QChar('a' + i));
    }

    QLabel *toLabel = new QLabel(tr("移動先:"), m_boardWidget);
    m_toFileCombo = new QComboBox(m_boardWidget);
    m_toRankCombo = new QComboBox(m_boardWidget);

    for (int i = 0; i < 9; ++i) {
        m_toFileCombo->addItem(kFileLabels[static_cast<size_t>(i)].toString(), i + 1);
    }
    for (int i = 0; i < 9; ++i) {
        m_toRankCombo->addItem(kRankLabels[static_cast<size_t>(i)].toString(), QChar('a' + i));
    }

    QLabel *promoteLabel = new QLabel(tr("成り:"), m_boardWidget);
    m_promoteCombo = new QComboBox(m_boardWidget);
    m_promoteCombo->addItem(tr("不成"), 0);
    m_promoteCombo->addItem(tr("成"), 1);

    boardLayout->addWidget(fromLabel);
    boardLayout->addWidget(m_fromFileCombo);
    boardLayout->addWidget(m_fromRankCombo);
    boardLayout->addSpacing(10);
    boardLayout->addWidget(toLabel);
    boardLayout->addWidget(m_toFileCombo);
    boardLayout->addWidget(m_toRankCombo);
    boardLayout->addSpacing(10);
    boardLayout->addWidget(promoteLabel);
    boardLayout->addWidget(m_promoteCombo);
    boardLayout->addStretch();

    inputLayout->addWidget(m_boardWidget);

    // 駒打ち用ウィジェット
    m_dropWidget = new QWidget(m_inputWidget);
    QHBoxLayout *dropLayout = new QHBoxLayout(m_dropWidget);
    dropLayout->setContentsMargins(0, 0, 0, 0);

    QLabel *pieceLabel = new QLabel(tr("駒種:"), m_dropWidget);
    m_dropPieceCombo = new QComboBox(m_dropWidget);
    for (size_t i = 0; i < kPieceLabels.size(); ++i) {
        m_dropPieceCombo->addItem(kPieceLabels[i].toString(), kPieceUsi[i].toString());
    }

    QLabel *dropToLabel = new QLabel(tr("打ち先:"), m_dropWidget);
    m_dropToFileCombo = new QComboBox(m_dropWidget);
    m_dropToRankCombo = new QComboBox(m_dropWidget);

    for (int i = 0; i < 9; ++i) {
        m_dropToFileCombo->addItem(kFileLabels[static_cast<size_t>(i)].toString(), i + 1);
    }
    for (int i = 0; i < 9; ++i) {
        m_dropToRankCombo->addItem(kRankLabels[static_cast<size_t>(i)].toString(), QChar('a' + i));
    }

    dropLayout->addWidget(pieceLabel);
    dropLayout->addWidget(m_dropPieceCombo);
    dropLayout->addSpacing(10);
    dropLayout->addWidget(dropToLabel);
    dropLayout->addWidget(m_dropToFileCombo);
    dropLayout->addWidget(m_dropToRankCombo);
    dropLayout->addStretch();

    inputLayout->addWidget(m_dropWidget);
    m_dropWidget->hide();

    layout->addWidget(m_inputWidget);

    // プレビュー行
    QHBoxLayout *previewLayout = new QHBoxLayout();
    QLabel *previewTitleLabel = new QLabel(tr("プレビュー:"), this);
    m_previewLabel = new QLabel(this);
    m_previewLabel->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 12pt; color: #333;"));

    QLabel *usiTitleLabel = new QLabel(tr("USI形式:"), this);
    m_usiLabel = new QLabel(this);
    m_usiLabel->setStyleSheet(QStringLiteral("font-family: monospace; color: #666;"));

    previewLayout->addWidget(previewTitleLabel);
    previewLayout->addWidget(m_previewLabel);
    previewLayout->addSpacing(20);
    previewLayout->addWidget(usiTitleLabel);
    previewLayout->addWidget(m_usiLabel);
    previewLayout->addStretch();

    layout->addLayout(previewLayout);

    // シグナル・スロット接続
    connect(m_typeGroup, &QButtonGroup::idClicked, this, &JosekiMoveInputWidget::onInputModeChanged);
    connect(m_fromFileCombo, &QComboBox::currentIndexChanged, this, &JosekiMoveInputWidget::onComboChanged);
    connect(m_fromRankCombo, &QComboBox::currentIndexChanged, this, &JosekiMoveInputWidget::onComboChanged);
    connect(m_toFileCombo, &QComboBox::currentIndexChanged, this, &JosekiMoveInputWidget::onComboChanged);
    connect(m_toRankCombo, &QComboBox::currentIndexChanged, this, &JosekiMoveInputWidget::onComboChanged);
    connect(m_dropPieceCombo, &QComboBox::currentIndexChanged, this, &JosekiMoveInputWidget::onComboChanged);
    connect(m_promoteCombo, &QComboBox::currentIndexChanged, this, &JosekiMoveInputWidget::onComboChanged);
    connect(m_dropToFileCombo, &QComboBox::currentIndexChanged, this, &JosekiMoveInputWidget::onComboChanged);
    connect(m_dropToRankCombo, &QComboBox::currentIndexChanged, this, &JosekiMoveInputWidget::onComboChanged);
}

void JosekiMoveInputWidget::onInputModeChanged()
{
    int id = m_typeGroup->checkedId();
    bool isNone = (id == 2);
    bool isDrop = (id == 1);

    if (m_noneRadio) {
        m_inputWidget->setVisible(!isNone);
    }

    if (!isNone) {
        m_boardWidget->setVisible(!isDrop);
        m_dropWidget->setVisible(isDrop);
    }

    updatePreview();
}

void JosekiMoveInputWidget::onComboChanged()
{
    updatePreview();
}

void JosekiMoveInputWidget::updatePreview()
{
    int id = m_typeGroup->checkedId();

    if (id == 2) {
        m_usiLabel->setText(QStringLiteral("none"));
        m_previewLabel->setText(tr("なし"));
        emit moveChanged();
        return;
    }

    QString usi = buildUsiMove();
    m_usiLabel->setText(usi);
    m_previewLabel->setText(usiToJapanese(usi));
    emit moveChanged();
}

QString JosekiMoveInputWidget::usiMove() const
{
    int id = m_typeGroup->checkedId();

    if (id == 2) {
        return QStringLiteral("none");
    }

    return buildUsiMove();
}

void JosekiMoveInputWidget::setUsiMove(const QString &usiMove)
{
    if (usiMove == QStringLiteral("none") || usiMove.isEmpty()) {
        if (m_noneRadio) {
            m_noneRadio->setChecked(true);
            onInputModeChanged();
        }
        return;
    }

    setComboFromUsi(usiMove);
}

QString JosekiMoveInputWidget::buildUsiMove() const
{
    int id = m_typeGroup->checkedId();
    bool isDrop = (id == 1);

    if (isDrop) {
        // 駒打ち: P*5e形式
        QString piece = m_dropPieceCombo->currentData().toString();
        int toFile = m_dropToFileCombo->currentData().toInt();
        QChar toRank = m_dropToRankCombo->currentData().toChar();

        return piece + QStringLiteral("*") + QString::number(toFile) + toRank;
    } else {
        // 盤上移動: 7g7f形式
        int fromFile = m_fromFileCombo->currentData().toInt();
        QChar fromRank = m_fromRankCombo->currentData().toChar();
        int toFile = m_toFileCombo->currentData().toInt();
        QChar toRank = m_toRankCombo->currentData().toChar();
        bool promote = m_promoteCombo->currentData().toInt() == 1;

        QString result = QString::number(fromFile) + fromRank +
                         QString::number(toFile) + toRank;
        if (promote) {
            result += QStringLiteral("+");
        }
        return result;
    }
}

void JosekiMoveInputWidget::setComboFromUsi(const QString &usiMove)
{
    if (usiMove.isEmpty()) return;

    // 駒打ちパターン: P*5e
    if (usiMove.size() >= 4 && usiMove.at(1) == QChar('*')) {
        m_dropRadio->setChecked(true);

        QChar piece = usiMove.at(0);
        int toFile = usiMove.at(2).toLatin1() - '0';
        int toRank = usiMove.at(3).toLatin1() - 'a';

        // 駒種を設定
        auto it = std::find(kPieceUsi.begin(), kPieceUsi.end(), QStringView(&piece, 1));
        if (it != kPieceUsi.end()) {
            m_dropPieceCombo->setCurrentIndex(static_cast<int>(std::distance(kPieceUsi.begin(), it)));
        }

        m_dropToFileCombo->setCurrentIndex(toFile - 1);
        m_dropToRankCombo->setCurrentIndex(toRank);

        onInputModeChanged();
        return;
    }

    // 通常移動パターン: 7g7f, 7g7f+
    if (usiMove.size() >= 4) {
        m_boardRadio->setChecked(true);

        int fromFile = usiMove.at(0).toLatin1() - '0';
        int fromRank = usiMove.at(1).toLatin1() - 'a';
        int toFile = usiMove.at(2).toLatin1() - '0';
        int toRank = usiMove.at(3).toLatin1() - 'a';
        bool promotes = (usiMove.size() >= 5 && usiMove.at(4) == QChar('+'));

        m_fromFileCombo->setCurrentIndex(fromFile - 1);
        m_fromRankCombo->setCurrentIndex(fromRank);
        m_toFileCombo->setCurrentIndex(toFile - 1);
        m_toRankCombo->setCurrentIndex(toRank);
        m_promoteCombo->setCurrentIndex(promotes ? 1 : 0);

        onInputModeChanged();
    }
}

bool JosekiMoveInputWidget::isValidUsiMove(const QString &move)
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
    return movePattern.match(move).hasMatch();
}

QString JosekiMoveInputWidget::usiToJapanese(const QString &usiMove)
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

void JosekiMoveInputWidget::applyFont(const QFont &baseFont)
{
    // ベースフォントを全子ウィジェットに適用
    setFont(baseFont);
    const auto children = findChildren<QWidget *>();
    for (auto *child : children) {
        child->setFont(baseFont);
    }

    // プレビューラベルは少し大きめに
    QFont previewFont = baseFont;
    previewFont.setPointSize(baseFont.pointSize() + 2);
    previewFont.setBold(true);
    m_previewLabel->setFont(previewFont);
}
