#ifndef KIFUDISPLAY_H
#define KIFUDISPLAY_H

/// @file kifudisplay.h
/// @brief 棋譜表示ウィジェットクラスの定義


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

    // コンストラクタ（4引数：コメント＋しおり付き）
    KifuDisplay(const QString &currentMove, const QString &timeSpent, const QString &comment, const QString &bookmark, QObject *parent = nullptr);

    // 指し手を取得する。
    QString currentMove() const;

    // 消費時間を取得する。
    QString timeSpent() const;

    // コメントを取得する。
    QString comment() const;

    // コメントを設定する。
    void setComment(const QString &comment);

    // しおりを取得する。
    QString bookmark() const;

    // しおりを設定する。
    void setBookmark(const QString &bookmark);

private:
    // 指し手
    QString m_currentMove;

    // 消費時間
    QString m_timeSpent;

    // コメント
    QString m_comment;

    // しおり
    QString m_bookmark;

};

#endif // KIFUDISPLAY_H
