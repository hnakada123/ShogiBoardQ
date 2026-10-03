/// @file jishogiscoredialog.cpp
/// @brief 持将棋点数・入玉宣言条件の比較表示

#include "jishogiscoredialog.h"

#include "buttonstyles.h"
#include "dialogutils.h"
#include "gamesettings.h"

#include <QApplication>
#include <QClipboard>
#include <QFontMetrics>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

namespace {
constexpr int kMinFontSize = 8;
constexpr int kMaxFontSize = 24;
constexpr QSize kDefaultSize{660, 640};
}

JishogiScoreDialog::JishogiScoreDialog(const JishogiCalculator::JishogiResult& result,
                                     bool senteInCheck, bool goteInCheck, QWidget* parent)
    : QDialog(parent)
    , m_fontHelper({GameSettings::jishogiScoreFontSize(), kMinFontSize, kMaxFontSize, 1,
                    GameSettings::setJishogiScoreFontSize})
{
    setObjectName(QStringLiteral("jishogiScoreDialog"));
    setWindowTitle(tr("持将棋の点数"));
    setMinimumSize(480, 360);
    resize(kDefaultSize);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    auto* scroll = new QScrollArea(this);
    scroll->setObjectName(QStringLiteral("scoreScrollArea"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    m_content = new QWidget(scroll);
    auto* contentLayout = new QVBoxLayout(m_content);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(12);

    const QString introduction = tr("表示中の局面で、各側が入玉宣言した場合の判定です。");
    auto* description = new QLabel(introduction, m_content);
    description->setWordWrap(true);
    contentLayout->addWidget(description);
    m_report << windowTitle() << introduction;

    auto* grid = new QGridLayout;
    grid->setSpacing(0);
    grid->setColumnStretch(0, 4);
    grid->setColumnStretch(1, 3);
    grid->setColumnStretch(2, 3);
    addRow(grid, tr("点数・宣言条件"), tr("先手"), tr("後手"), QStringLiteral("header"), true);
    addRow(grid, tr("宣言点数\n敵陣の駒 ＋ 持ち駒"),
           tr("%1点").arg(result.sente.declarationPoints),
           tr("%1点").arg(result.gote.declarationPoints), QStringLiteral("declarationPoints"), true);
    addRow(grid, tr("総点数\n盤上の全駒 ＋ 持ち駒"),
           tr("%1点").arg(result.sente.totalPoints),
           tr("%1点").arg(result.gote.totalPoints), QStringLiteral("totalPoints"));
    addRow(grid, tr("玉が敵陣にいる"), conditionText(result.sente.kingInEnemyTerritory),
           conditionText(result.gote.kingInEnemyTerritory), QStringLiteral("kingInCamp"));
    addRow(grid, tr("敵陣に玉以外の駒が10枚以上"),
           tr("%1\n%2 / 10枚").arg(conditionText(result.sente.piecesInEnemyTerritory >= 10))
               .arg(result.sente.piecesInEnemyTerritory),
           tr("%1\n%2 / 10枚").arg(conditionText(result.gote.piecesInEnemyTerritory >= 10))
               .arg(result.gote.piecesInEnemyTerritory), QStringLiteral("piecesInCamp"));
    addRow(grid, tr("王手がかかっていない"), conditionText(!senteInCheck),
           conditionText(!goteInCheck), QStringLiteral("notInCheck"));
    addRow(grid, tr("24点法"), ruleText(result.sente, senteInCheck, true, true),
           ruleText(result.gote, goteInCheck, false, true), QStringLiteral("rule24"), true);
    addRow(grid, tr("27点法"), ruleText(result.sente, senteInCheck, true, false),
           ruleText(result.gote, goteInCheck, false, false), QStringLiteral("rule27"), true);
    contentLayout->addLayout(grid);

    const QString notes = tr("点数：飛・角（龍・馬）は5点、その他は1点。玉は数えません。\n"
                             "敵陣：先手は一〜三段、後手は七〜九段。\n"
                             "24点法：24〜30点で引き分け、31点以上で勝ち。\n"
                             "27点法：先手28点以上、後手27点以上で勝ち。\n"
                             "どちらも上の3つの宣言条件を満たす必要があります。実際の宣言は自分の手番で行います。");
    auto* explanation = new QLabel(notes, m_content);
    explanation->setWordWrap(true);
    explanation->setTextInteractionFlags(Qt::TextSelectableByMouse);
    contentLayout->addWidget(explanation);
    contentLayout->addStretch();
    m_report << QString() << notes;
    scroll->setWidget(m_content);
    mainLayout->addWidget(scroll, 1);
    mainLayout->addLayout(createButtons());
    applyFontSize();

    // 旧レイアウト用の小さい保存サイズは比較表の標準サイズへ移行する。
    const QSize savedSize = GameSettings::jishogiScoreDialogSize();
    if (savedSize.width() >= minimumWidth() && savedSize.height() >= minimumHeight()) {
        DialogUtils::restoreDialogSize(this, savedSize);
    }
}

JishogiScoreDialog::~JishogiScoreDialog()
{
    GameSettings::setJishogiScoreFontSize(m_fontHelper.fontSize());
    DialogUtils::saveDialogSize(this, GameSettings::setJishogiScoreDialogSize);
}

QHBoxLayout* JishogiScoreDialog::createButtons()
{
    auto* buttons = new QHBoxLayout;
    buttons->setSpacing(8);
    m_fontDecrease = new QPushButton(tr("A-"), this);
    m_fontDecrease->setObjectName(QStringLiteral("fontDecrease"));
    m_fontDecrease->setToolTip(tr("文字を小さくする"));
    m_fontDecrease->setAccessibleName(m_fontDecrease->toolTip());
    m_fontIncrease = new QPushButton(tr("A+"), this);
    m_fontIncrease->setObjectName(QStringLiteral("fontIncrease"));
    m_fontIncrease->setToolTip(tr("文字を大きくする"));
    m_fontIncrease->setAccessibleName(m_fontIncrease->toolTip());
    for (auto* button : {m_fontDecrease, m_fontIncrease}) {
        button->setStyleSheet(ButtonStyles::fontButton());
        button->setFixedWidth(40);
        button->setAutoDefault(false);
        buttons->addWidget(button);
    }
    m_fontSizeLabel = new QLabel(this);
    buttons->addWidget(m_fontSizeLabel);
    buttons->addStretch();
    auto* copy = new QPushButton(tr("結果をコピー"), this);
    copy->setObjectName(QStringLiteral("copyReport"));
    copy->setStyleSheet(ButtonStyles::editOperation());
    copy->setAutoDefault(false);
    buttons->addWidget(copy);
    auto* close = new QPushButton(tr("閉じる"), this);
    close->setObjectName(QStringLiteral("closeButton"));
    close->setStyleSheet(ButtonStyles::primaryAction());
    close->setDefault(true);
    buttons->addWidget(close);
    connect(m_fontDecrease, &QPushButton::clicked, this, &JishogiScoreDialog::decreaseFontSize);
    connect(m_fontIncrease, &QPushButton::clicked, this, &JishogiScoreDialog::increaseFontSize);
    connect(copy, &QPushButton::clicked, this, &JishogiScoreDialog::copyReport);
    connect(close, &QPushButton::clicked, this, &QDialog::accept);
    return buttons;
}

void JishogiScoreDialog::addRow(QGridLayout* grid, const QString& title, const QString& sente,
                               const QString& gote, const QString& name, bool emphasis)
{
    const int row = grid->rowCount();
    const QStringList texts{title, sente, gote};
    const QStringList prefixes{QStringLiteral("label_"), QStringLiteral("sente_"), QStringLiteral("gote_")};
    for (int column = 0; column < 3; ++column) {
        auto* label = new QLabel(texts.at(column), m_content);
        label->setObjectName(prefixes.at(column) + name);
        label->setTextFormat(Qt::PlainText);
        label->setWordWrap(true);
        label->setMargin(10);
        label->setAlignment(column == 0 ? Qt::AlignVCenter | Qt::AlignLeft : Qt::AlignCenter);
        label->setTextInteractionFlags(Qt::TextSelectableByMouse);
        label->setAccessibleName(title + QStringLiteral(": ") + texts.at(column));
        label->setProperty("emphasis", emphasis);
        label->setProperty("valueCell", column > 0);
        label->setProperty("score", name == QLatin1String("declarationPoints") && column > 0);
        label->setStyleSheet(QStringLiteral("QLabel { background: palette(%1); border-bottom: 1px solid palette(mid); }")
                            .arg(emphasis ? QStringLiteral("alternate-base") : QStringLiteral("base")));
        grid->addWidget(label, row, column);
    }
    m_report << QStringLiteral("%1\t%2\t%3")
                    .arg(QString(title).replace(QLatin1Char('\n'), QLatin1Char(' ')),
                         QString(sente).replace(QLatin1Char('\n'), QLatin1Char(' ')),
                         QString(gote).replace(QLatin1Char('\n'), QLatin1Char(' ')));
}

QString JishogiScoreDialog::conditionText(bool met)
{
    return met ? tr("○ 達成") : tr("× 未達");
}

QString JishogiScoreDialog::ruleText(const JishogiCalculator::PlayerScore& score,
                                    bool inCheck, bool isSente, bool rule24)
{
    const int requiredPoints = rule24 ? 24 : (isSente ? 28 : 27);
    QString text;
    if (!JishogiCalculator::meetsDeclarationConditions(score, inCheck)) {
        text = tr("条件未達");
    } else if (score.declarationPoints < requiredPoints) {
        text = tr("点数不足");
    } else {
        text = tr("宣言時：%1").arg(rule24 ? JishogiCalculator::result24(score, inCheck)
                                            : JishogiCalculator::result27(score, isSente, inCheck));
    }
    if (score.declarationPoints < requiredPoints) {
        text += QLatin1Char('\n') + tr("あと%1点").arg(requiredPoints - score.declarationPoints);
    }
    return text;
}

void JishogiScoreDialog::applyFontSize()
{
    DialogUtils::standardizeDialog(this);
    QFont contentFont = font();
    contentFont.setPointSize(m_fontHelper.fontSize());
    DialogUtils::applyFontToAllChildren(m_content, contentFont);
    const auto labels = m_content->findChildren<QLabel*>();
    for (auto* label : labels) {
        QFont labelFont = contentFont;
        labelFont.setBold(label->property("emphasis").toBool());
        if (label->property("score").toBool()) labelFont.setPointSize(contentFont.pointSize() + 4);
        label->setFont(labelFont);
        if (label->property("valueCell").toBool() || label->objectName() == QLatin1String("label_header")) {
            // 拡大時も判定語・点数を途中で折り返さず、必要なら横スクロールする。
            const QFontMetrics metrics(labelFont);
            int textWidth = 0;
            const auto lines = label->text().split(QLatin1Char('\n'));
            for (const auto& line : lines) textWidth = qMax(textWidth, metrics.horizontalAdvance(line));
            label->setMinimumWidth(textWidth + 2 * label->margin() + 2);
        }
    }
    m_fontDecrease->setEnabled(m_fontHelper.fontSize() > kMinFontSize);
    m_fontIncrease->setEnabled(m_fontHelper.fontSize() < kMaxFontSize);
    m_fontSizeLabel->setText(tr("%1 pt").arg(m_fontHelper.fontSize()));
    DialogUtils::updateFontButtons(this, m_fontHelper.fontSize());
}

void JishogiScoreDialog::decreaseFontSize()
{
    if (m_fontHelper.decrease()) applyFontSize();
}

void JishogiScoreDialog::increaseFontSize()
{
    if (m_fontHelper.increase()) applyFontSize();
}

void JishogiScoreDialog::copyReport()
{
    QApplication::clipboard()->setText(m_report.join(QLatin1Char('\n')));
}
