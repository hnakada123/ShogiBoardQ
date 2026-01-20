#ifndef NYUGYOKUDECLARATIONHANDLER_H
#define NYUGYOKUDECLARATIONHANDLER_H

#include <QObject>
#include <QWidget>

class ShogiBoard;
class ShogiGameController;
class MatchCoordinator;

/**
 * @brief 入玉宣言の処理を管理するハンドラ
 *
 * MainWindowから分離された責務:
 * - 入玉宣言の条件判定
 * - 宣言結果の表示
 * - 対局終了処理への連携
 */
class NyugyokuDeclarationHandler : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 入玉宣言の結果
     */
    struct DeclarationResult {
        bool success = false;        ///< 宣言成功
        bool isDraw = false;         ///< 引き分け（持将棋）
        QString resultText;          ///< 結果テキスト（"宣言勝ち"等）
        QString conditionDetails;    ///< 条件判定の詳細
    };

    explicit NyugyokuDeclarationHandler(QObject* parent = nullptr);

    /**
     * @brief 依存オブジェクトを設定
     */
    void setGameController(ShogiGameController* gc);
    void setMatchCoordinator(MatchCoordinator* match);

    /**
     * @brief 入玉宣言を実行する
     * @param parentWidget 親ウィジェット（ダイアログの親）
     * @param board 現在の盤面データ
     * @param playMode 現在のプレイモード
     * @return 宣言が実行されたかどうか（キャンセル時はfalse）
     */
    bool handleDeclaration(QWidget* parentWidget, ShogiBoard* board, int playMode);

signals:
    /**
     * @brief 宣言結果を通知
     */
    void declarationCompleted(const DeclarationResult& result);

private:
    /**
     * @brief 宣言条件の判定文字列を生成
     */
    QString buildConditionDetails(bool kingInEnemyTerritory, int piecesInEnemyTerritory,
                                   bool noCheck, int declarationPoints, int jishogiRule,
                                   bool isSente) const;

    /**
     * @brief 24点法での結果判定
     */
    DeclarationResult judge24PointRule(bool kingInEnemyTerritory, bool enoughPieces,
                                        bool noCheck, int declarationPoints) const;

    /**
     * @brief 27点法での結果判定
     */
    DeclarationResult judge27PointRule(bool kingInEnemyTerritory, bool enoughPieces,
                                        bool noCheck, int declarationPoints, bool isSente) const;

    ShogiGameController* m_gameController = nullptr;
    MatchCoordinator* m_match = nullptr;
};

#endif // NYUGYOKUDECLARATIONHANDLER_H
