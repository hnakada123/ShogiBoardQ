/// @file automationactionpolicy.cpp
/// @brief 自動化 API から実行してよい QAction の許可リストの実装

#include "automationactionpolicy.h"

const QStringList& AutomationActionPolicy::allowedActions()
{
    static const QStringList actions = {
        // ファイル・棋譜
        QStringLiteral("actionOpenKifuFile"),
        QStringLiteral("actionSaveAs"),
        QStringLiteral("actionPasteKifu"),
        QStringLiteral("actionSfenCollectionViewer"),
        QStringLiteral("actionSaveBoardImage"),
        QStringLiteral("actionSaveEvaluationGraph"),
        QStringLiteral("actionCopyKIF"),
        QStringLiteral("actionCopyKI2"),
        QStringLiteral("actionCopyCSA"),
        QStringLiteral("actionCopyJKF"),
        QStringLiteral("actionCopyUSEN"),
        QStringLiteral("actionCopyUSIAll"),
        QStringLiteral("actionCopyUSICurrent"),
        QStringLiteral("actionCopySFEN"),
        QStringLiteral("actionCopyBOD"),
        QStringLiteral("actionCopyBoardToClipboard"),
        QStringLiteral("actionCopyEvalGraphToClipboard"),
        // 対局
        QStringLiteral("actionNewGame"),
        QStringLiteral("actionStartGame"),
        QStringLiteral("actionResign"),
        QStringLiteral("actionBreakOffGame"),
        QStringLiteral("actionUndoMove"),
        QStringLiteral("actionMakeImmediateMove"),
        QStringLiteral("actionCSA"),
        QStringLiteral("actionJishogiScore"),
        QStringLiteral("actionNyugyokuDeclaration"),
        // 解析・詰将棋
        QStringLiteral("actionAnalyzeKifu"),
        QStringLiteral("actionCancelAnalyzeKifu"),
        QStringLiteral("actionTsumeShogiSearch"),
        QStringLiteral("actionStopTsumeSearch"),
        QStringLiteral("actionTsumeshogiGenerator"),
        QStringLiteral("actionTsumePlay"),
        // 局面編集
        QStringLiteral("actionStartEditPosition"),
        QStringLiteral("actionEndEditPosition"),
        QStringLiteral("actionSetHiratePosition"),
        QStringLiteral("actionSetTsumePosition"),
        QStringLiteral("actionReturnAllPiecesToStand"),
        QStringLiteral("actionChangeTurn"),
        // 表示
        QStringLiteral("actionFlipBoard"),
        QStringLiteral("actionEnlargeBoard"),
        QStringLiteral("actionShrinkBoard"),
        QStringLiteral("actionToolBar"),
        QStringLiteral("actionLockDocks"),
        QStringLiteral("actionMenuWindow"),
        QStringLiteral("actionPieceStyleStandard"),
        QStringLiteral("actionPieceStyleWood"),
        QStringLiteral("actionPieceStyleIvory"),
        QStringLiteral("actionPieceStyleDark"),
        QStringLiteral("actionPieceStyleClear"),
        QStringLiteral("actionBoardColors"),
        QStringLiteral("actionPieceSound"),
        QStringLiteral("actionPieceSoundSettings"),
        // 設定・情報
        QStringLiteral("actionEngineSettings"),
        QStringLiteral("actionVersionInfo"),
        QStringLiteral("actionUsage"),
    };
    return actions;
}

bool AutomationActionPolicy::isAllowed(const QString& objectName)
{
    return allowedActions().contains(objectName);
}
