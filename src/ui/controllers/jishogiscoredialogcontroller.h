#ifndef JISHOGISCOREDIALOGCONTROLLER_H
#define JISHOGISCOREDIALOGCONTROLLER_H

/// @file jishogiscoredialogcontroller.h
/// @brief 持将棋スコアダイアログコントローラクラスの定義
/// @todo remove コメントスタイルガイド適用済み


#include <QObject>
#include <QWidget>

class ShogiBoard;

/**
 * @brief 持将棋点数ダイアログの表示を管理するコントローラ
 *
 * MainWindowから分離された責務:
 * - 持将棋の点数計算結果の表示
 * - 宣言条件の判定表示
 * - ダイアログのフォントサイズとウィンドウサイズの保存/復元
 */
class JishogiScoreDialogController : public QObject
{
    Q_OBJECT

public:
    explicit JishogiScoreDialogController(QObject* parent = nullptr);

    /**
     * @brief 持将棋点数ダイアログを表示する
     * @param parentWidget 親ウィジェット（ダイアログの親）
     * @param board 現在の盤面データ
     */
    void showDialog(QWidget* parentWidget, ShogiBoard* board);

private:
    /**
     * @brief 宣言条件の判定文字列を生成する
     * @param kingInEnemyTerritory 玉が敵陣にいるか
     * @param piecesInEnemyTerritory 敵陣にある駒数
     * @param inCheck 王手がかかっているか
     * @param declarationPoints 宣言点数
     * @return 判定結果の文字列
     */
    QString buildConditionString(bool kingInEnemyTerritory, int piecesInEnemyTerritory,
                                  bool inCheck, int declarationPoints) const;
};

#endif // JISHOGISCOREDIALOGCONTROLLER_H
