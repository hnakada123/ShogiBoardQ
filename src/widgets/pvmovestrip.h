#ifndef PVMOVESTRIP_H
#define PVMOVESTRIP_H

/// @file pvmovestrip.h
/// @brief 読み筋の手を並べ、表示中の局面の手を示す帯（読み筋盤の上部）

#include <QString>
#include <QStringList>
#include <QWidget>

class QButtonGroup;

/**
 * @brief 読み筋の局面ごとの語を、押せる部品にして折り返して並べる帯
 *
 * 1語が1局面に対応する（labels の先頭が開始局面）。表示中の局面の語を塗り、そこまでの語は濃く、
 * この先の語は薄く出す。語は途中で折り返さず、塗っても幅が変わらないので、局面を送っても語の位置は動かない。
 * 押された語は positionRequested() で知らせるだけで、表示する局面は呼び出し側が決めて setCurrent() で返す。
 * 語の部品の名前は "pvMove<番号>"。今の局面の語は checked になる。
 */
class PvMoveStrip : public QWidget
{
    Q_OBJECT

public:
    /**
     * @param labels 局面ごとの語（[0] が開始局面、[i] が i 手目）
     * @param ending 手の後に続く語（末尾の注記・投了など。局面を伴わないので押せず、塗らない。文字のまま出す）
     */
    explicit PvMoveStrip(const QStringList& labels, const QString& ending, QWidget* parent = nullptr);

    QSize sizeHint() const override;

public slots:
    /// 表示中の局面（範囲外なら何もしない）
    void setCurrent(int index);

signals:
    /// 語が押された
    void positionRequested(int index);

private:
    QButtonGroup* m_buttons = nullptr;
};

#endif // PVMOVESTRIP_H
