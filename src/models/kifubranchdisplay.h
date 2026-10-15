#ifndef KIFUBRANCHDISPLAY_H
#define KIFUBRANCHDISPLAY_H

/// @file kifubranchdisplay.h
/// @brief 棋譜分岐候補表示ウィジェットクラスの定義


#include <QObject>
#include <QString>
#include "kifupresentation.h"

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
    QString beforeSfen;
    QString usiMove;
    QString displayMove(bool fullOrigin = false) const
    { return KifuPresentation::label(m_currentMove, beforeSfen, usiMove, fullOrigin); }

    void setCurrentMove(const QString &newCurrentMove);

private:
    // 指し手
    QString m_currentMove;
};

#endif // KIFUBRANCHDISPLAY_H
