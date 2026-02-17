/// @file jishogiscoredialogcontroller.cpp
/// @brief 持将棋スコアダイアログコントローラクラスの実装

#include "jishogiscoredialogcontroller.h"
#include "buttonstyles.h"
#include "shogiboard.h"
#include "jishogicalculator.h"
#include "movevalidatorselector.h"
#include "settingsservice.h"

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

JishogiScoreDialogController::JishogiScoreDialogController(QObject* parent)
    : QObject(parent)
{
}

QString JishogiScoreDialogController::buildConditionString(
    bool kingInEnemyTerritory, int piecesInEnemyTerritory,
    bool inCheck, int declarationPoints) const
{
    const QString checkMark = QStringLiteral("○");
    const QString crossMark = QStringLiteral("×");

    QString str;
    str += tr("【宣言条件】") + QStringLiteral("\n");
    str += tr("① 玉が敵陣 : %1").arg(kingInEnemyTerritory ? checkMark : crossMark) + QStringLiteral("\n");
    str += tr("② 敵陣10枚以上 : %1 (%2枚)")
               .arg(piecesInEnemyTerritory >= 10 ? checkMark : crossMark)
               .arg(piecesInEnemyTerritory) + QStringLiteral("\n");
    str += tr("③ 王手なし : %1").arg(!inCheck ? checkMark : crossMark) + QStringLiteral("\n");
    str += tr("④ 宣言点数 : %1点").arg(declarationPoints);
    return str;
}

void JishogiScoreDialogController::showDialog(QWidget* parentWidget, ShogiBoard* board)
{
    if (!board) {
        return;
    }

    auto result = JishogiCalculator::calculate(board->boardData(), board->getPieceStand());

    // 王手判定のためのMoveValidatorを作成
    MoveValidatorSelector::ActiveMoveValidator validator;

    // 先手の玉が王手されているかどうかを判定
    bool senteInCheck = validator.checkIfKingInCheck(MoveValidatorSelector::ActiveMoveValidator::BLACK, board->boardData()) > 0;

    // 後手の玉が王手されているかどうかを判定
    bool goteInCheck = validator.checkIfKingInCheck(MoveValidatorSelector::ActiveMoveValidator::WHITE, board->boardData()) > 0;

    // 宣言条件の文字列を生成
    QString senteCondition = buildConditionString(
        result.sente.kingInEnemyTerritory,
        result.sente.piecesInEnemyTerritory,
        senteInCheck,
        result.sente.declarationPoints);

    QString goteCondition = buildConditionString(
        result.gote.kingInEnemyTerritory,
        result.gote.piecesInEnemyTerritory,
        goteInCheck,
        result.gote.declarationPoints);

    QString message = tr("持将棋の点数\n\n"
                         "先手\n"
                         "%1\n"
                         "24点法 : %2\n"
                         "27点法 : %3\n\n"
                         "後手\n"
                         "%4\n"
                         "24点法 : %5\n"
                         "27点法 : %6")
        .arg(senteCondition,
             JishogiCalculator::getResult24(result.sente, senteInCheck),
             JishogiCalculator::getResult27(result.sente, true, senteInCheck),
             goteCondition,
             JishogiCalculator::getResult24(result.gote, goteInCheck),
             JishogiCalculator::getResult27(result.gote, false, goteInCheck));

    // カスタムダイアログを作成
    QDialog dialog(parentWidget);
    dialog.setWindowTitle(tr("持将棋の点数"));

    // 保存されたウィンドウサイズを読み込む
    QSize savedSize = SettingsService::jishogiScoreDialogSize();
    if (savedSize.isValid() && savedSize.width() > 50 && savedSize.height() > 50) {
        dialog.resize(savedSize);
    }

    QVBoxLayout* mainLayout = new QVBoxLayout(&dialog);

    // テキスト表示用ラベル
    QLabel* label = new QLabel(message);
    label->setAlignment(Qt::AlignLeft);

    // 保存されたフォントサイズを読み込む
    int savedFontSize = SettingsService::jishogiScoreFontSize();
    QFont labelFont = label->font();
    labelFont.setPointSize(savedFontSize);
    label->setFont(labelFont);

    mainLayout->addWidget(label);

    // ボタン用レイアウト
    QHBoxLayout* buttonLayout = new QHBoxLayout();

    // A- ボタン（文字縮小）
    QPushButton* shrinkButton = new QPushButton(tr("A-"));
    shrinkButton->setFixedWidth(40);
    shrinkButton->setStyleSheet(ButtonStyles::fontButton());
    connect(shrinkButton, &QPushButton::clicked, [label]() {
        QFont font = label->font();
        if (font.pointSize() > 6) {
            font.setPointSize(font.pointSize() - 1);
            label->setFont(font);
        }
    });
    buttonLayout->addWidget(shrinkButton);

    // A+ ボタン（文字拡大）
    QPushButton* enlargeButton = new QPushButton(tr("A+"));
    enlargeButton->setFixedWidth(40);
    enlargeButton->setStyleSheet(ButtonStyles::fontButton());
    connect(enlargeButton, &QPushButton::clicked, [label]() {
        QFont font = label->font();
        font.setPointSize(font.pointSize() + 1);
        label->setFont(font);
    });
    buttonLayout->addWidget(enlargeButton);

    buttonLayout->addStretch();

    // OKボタン
    QPushButton* okButton = new QPushButton(tr("OK"));
    okButton->setStyleSheet(ButtonStyles::primaryAction());
    connect(okButton, &QPushButton::clicked, &dialog, &QDialog::accept);
    buttonLayout->addWidget(okButton);

    mainLayout->addLayout(buttonLayout);

    dialog.exec();

    // ダイアログを閉じる際にフォントサイズとウィンドウサイズを保存
    SettingsService::setJishogiScoreFontSize(label->font().pointSize());
    SettingsService::setJishogiScoreDialogSize(dialog.size());
}
