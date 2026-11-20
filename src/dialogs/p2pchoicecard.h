#ifndef P2PCHOICECARD_H
#define P2PCHOICECARD_H

/// @file p2pchoicecard.h
/// @brief 選択肢の1つを、選ぶと何が起きるかの説明と一緒に示す枠

#include <QFrame>
#include <QList>
#include <QString>

class QGridLayout;
class QLabel;
class QRadioButton;

/**
 * @brief 選択肢の1つを、選ぶと何が起きるかの説明と一緒に示す枠
 *
 * 見出しはラジオボタンで、枠のどこを押しても選べる。説明は「項目・内容」の行で出す（項目が空の行は、内容だけを
 * 幅いっぱいに出す）。チェックだけでは、選ばなかった場合に何が起きるかが分からないので、選択肢を並べて、どちらにも
 * 説明を付けるために使う。排他にするのは使う側（radio() を QButtonGroup に入れる）。
 */
class P2pChoiceCard : public QFrame
{
    Q_OBJECT
public:
    struct Row {
        QString label;   ///< 項目（空なら、内容だけを幅いっぱいに出す）
        QString value;   ///< 内容（折り返す）
    };

    explicit P2pChoiceCard(const QString& title, QWidget* parent = nullptr);

    QRadioButton* radio() const { return m_radio; }
    /// 説明の行を置く（行数が同じなら、文だけを入れ替える）
    void setRows(const QList<Row>& rows);
    /// 押しても選択を変えない（接続している間など）
    void setLocked(bool locked);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private slots:
    void onToggled();

private:
    QRadioButton* m_radio = nullptr;
    QGridLayout* m_grid = nullptr;
    QList<QLabel*> m_labels;   ///< 行ごとに、項目・内容の順
    bool m_locked = false;
};

#endif // P2PCHOICECARD_H
