#ifndef ELIDELABEL_H
#define ELIDELABEL_H

/// @file elidelabel.h
/// @brief テキスト省略表示ラベルクラスの定義


#include <QLabel>
#include <QStringList>

/**
 * @brief 幅に収まらないテキストをエリプシス（…）で省略して描く QLabel 派生クラス
 *
 * 全文は setFullText で渡し、ツールチップとプロパティ "fullText" にも入れる。
 * wordWrap が有効なら2行に分け、収まらない最後の行だけを省略する。
 */
class ElideLabel : public QLabel
{
    Q_OBJECT
public:
    explicit ElideLabel(QWidget* parent = nullptr);

    /// 表示の元になる全文を設定する（同じ文字列なら何もしない）
    void setFullText(const QString& text);
    QString fullText() const;

    /// 省略する位置（左・中・右）
    void setElideMode(Qt::TextElideMode mode);
    Qt::TextElideMode elideMode() const;

protected:
    void paintEvent(QPaintEvent*) override;

private:
    /// wordWrap 有効時は2行に分ける
    QStringList displayLines() const;

    QString m_fullText;
    Qt::TextElideMode m_mode = Qt::ElideMiddle;
};

#endif // ELIDELABEL_H
