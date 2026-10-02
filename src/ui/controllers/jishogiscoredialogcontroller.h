#ifndef JISHOGISCOREDIALOGCONTROLLER_H
#define JISHOGISCOREDIALOGCONTROLLER_H

/// @file jishogiscoredialogcontroller.h
/// @brief 持将棋スコアダイアログコントローラクラスの定義


#include <QObject>

class ShogiBoard;
class QWidget;

/**
 * @brief 持将棋点数ダイアログの表示を管理するコントローラ
 *
 * MainWindowから分離された責務:
 * - 持将棋の点数計算結果の表示
 * - 王手判定と表示専用ダイアログの起動
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

};

#endif // JISHOGISCOREDIALOGCONTROLLER_H
