#ifndef NYUGYOKUDECLARATIONHANDLER_H
#define NYUGYOKUDECLARATIONHANDLER_H

/// @file nyugyokudeclarationhandler.h
/// @brief 入玉宣言ハンドラクラスの定義


#include <QObject>

class ShogiBoard;
class ShogiGameController;
class MatchCoordinator;
class QWidget;

/**
 * @brief 入玉宣言の処理を管理するハンドラ
 *
 * 責務:
 * - 入玉宣言の確認（条件の判定は NyugyokuJudgement）
 * - 宣言結果の表示
 * - 対局終了処理への連携
 */
class NyugyokuDeclarationHandler : public QObject
{
    Q_OBJECT

public:
    /// gc: 宣言する側（手番）、match: ローカル対局の終局を伝える先
    NyugyokuDeclarationHandler(ShogiGameController* gc, MatchCoordinator* match, QObject* parent = nullptr);

    /**
     * @brief 入玉宣言を実行する
     * @param parentWidget 親ウィジェット（ダイアログの親）
     * @param board 現在の盤面データ
     * @param inLocalGame ローカルの対局中か
     * @return 宣言が実行されたかどうか（キャンセル時はfalse）
     */
    bool handleDeclaration(QWidget* parentWidget, ShogiBoard* board, bool inLocalGame);

private:
    ShogiGameController* m_gameController = nullptr;
    MatchCoordinator* m_match = nullptr;
};

#endif // NYUGYOKUDECLARATIONHANDLER_H
