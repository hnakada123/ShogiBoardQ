#ifndef KIFUBRANCHDISPLAY_H
#define KIFUBRANCHDISPLAY_H

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

private:
    // 指し手
    QString m_currentMove;
};

#endif // KIFUBRANCHDISPLAY_H
