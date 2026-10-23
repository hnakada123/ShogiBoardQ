#ifndef JOSEKIMOVEINPUTWIDGET_H
#define JOSEKIMOVEINPUTWIDGET_H

/// @file josekimoveinputwidget.h
/// @brief 定跡手入力ウィジェットクラスの定義

#include <QGroupBox>

class QComboBox;
class QLabel;

/**
 * @brief 定跡手入力ウィジェット
 *
 * 局面の合法手を棋譜の表記（「▲７六歩(77)」）で並べ、その中から選ばせる（指せない手は入力できない）。
 * 「指し手」と「予想応手」の両方に使う。予想応手には先頭に「なし」を置ける。
 */
class JosekiMoveInputWidget : public QGroupBox
{
    Q_OBJECT

public:
    /**
     * @param name オブジェクト名（指し手の一覧は "<name>Combo"）
     * @param title グループボックスのタイトル
     * @param hasNoneOption 先頭に「なし」を置くか
     */
    JosekiMoveInputWidget(const QString &name, const QString &title, bool hasNoneOption, QWidget *parent = nullptr);

    /// 「なし」の項目の値（usiMove・setUsiMove で使う）
    static inline const QString kNoMove = QStringLiteral("none");

    /// 選んだ指し手（USI。「なし」は kNoMove、選んでいなければ空）
    QString usiMove() const;

    /// usiMove を選ぶ（kNoMove は「なし」）。今の局面の合法手になければ、局面が変わったときに選び直す
    void setUsiMove(const QString &usiMove);

    /// 合法手を並べる局面（空なら何も並べない）。選んでいた手が合法手にあれば選んだままにする
    void setCurrentSfen(const QString &sfen);

signals:
    /// 選んだ指し手が変わった
    void moveChanged();

private slots:
    void onCurrentIndexChanged();

private:
    void populate();
    void selectRequested();

    QComboBox *m_combo = nullptr;
    QLabel *m_previewLabel = nullptr;
    bool m_hasNoneOption = false;
    QString m_currentSfen;
    QString m_requested; ///< 選びたい指し手（局面が変わっても合法手なら選び直す）
};

#endif // JOSEKIMOVEINPUTWIDGET_H
