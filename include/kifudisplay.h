#ifndef KIFUDISPLAY_H
#define KIFUDISPLAY_H

#include <QObject>
#include <QString>

// 棋譜欄を表示するクラス
class KifuDisplay : public QObject
{
    Q_OBJECT

public:
    // コンストラクタ
    explicit KifuDisplay(QObject *parent = nullptr);

    // コンストラクタ
    KifuDisplay(const QString &currentMove, const QString &timeSpent, QObject *parent = nullptr);

    // 指し手を取得する。
    QString currentMove() const;

    // 消費時間を取得する。  
    QString timeSpent() const;

private:
    // 指し手
    QString m_currentMove;

    // 消費時間
    QString m_timeSpent;

};

#endif // KIFUDISPLAY_H
