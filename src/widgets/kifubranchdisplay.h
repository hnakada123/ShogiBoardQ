#ifndef KIFUBRANCHDISPLAY_H
#define KIFUBRANCHDISPLAY_H

/// @file kifubranchdisplay.h
/// @brief 棋譜分岐候補表示ウィジェットクラスの定義
/// @todo remove コメントスタイルガイド適用済み


#include <QObject>
#include <QString>

// 棋譜欄を表示するクラス
class KifuBranchDisplay : public QObject
{
    Q_OBJECT

public:
    // コンストラクタ
    explicit KifuBranchDisplay(QObject *parent = nullptr);

    // コンストラクタ
    KifuBranchDisplay(const QString &currentMove, QObject *parent = nullptr);

    // 指し手を取得する。
    QString currentMove() const;

    void setCurrentMove(const QString &newCurrentMove);

private:
    // 指し手
    QString m_currentMove;
};

#endif // KIFUBRANCHDISPLAY_H
