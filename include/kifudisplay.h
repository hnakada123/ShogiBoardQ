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

    // コンストラクタ（2引数）
    KifuDisplay(const QString &currentMove, const QString &timeSpent, QObject *parent = nullptr);

    // コンストラクタ（3引数：コメント付き）
    KifuDisplay(const QString &currentMove, const QString &timeSpent, const QString &comment, QObject *parent = nullptr);

    // 指し手を取得する。
    QString currentMove() const;

    // 消費時間を取得する。
    QString timeSpent() const;

    // コメントを取得する。
    QString comment() const;

    // コメントを設定する。
    void setComment(const QString &comment);

private:
    // 指し手
    QString m_currentMove;

    // 消費時間
    QString m_timeSpent;

    // コメント
    QString m_comment;

};

#endif // KIFUDISPLAY_H
