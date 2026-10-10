#ifndef DIALOGFONTSCALE_H
#define DIALOGFONTSCALE_H

#include <QPointer>
#include <QWidget>

class QCoreApplication;
class QDialog;
class QLabel;
class QLayout;

/// 補助ダイアログに文字サイズ操作と保存を追加する。
/// 独自描画の盤面などは dialogFontScaleExcluded プロパティで対象外にできる。
class DialogFontScale : public QWidget
{
public:
    static void install(QDialog* dialog, const QString& settingsId, bool saveSize = false);

    /// 以後に表示するすべての QMessageBox（静的関数で出すものを含む）に文字サイズ操作を付ける。
    /// 文字サイズはメッセージボックス全体で共有して保存する。
    static void installForMessageBoxes(QCoreApplication* app);

    /// installForMessageBoxes で付けた操作が、表示時にメッセージボックスへ適用する文字（保存済みの大きさ）を返す。
    /// 表示前に本文の幅を測るときに使う。
    static QFont messageBoxFont(const QFont& font);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    DialogFontScale(QDialog* dialog, const QString& settingsId, bool saveSize);
    void increase();
    void decrease();
    void applySize(int size);
    void updateLayout();
    bool attachToLayout();
    void ensureContentFits();

    QDialog* m_dialog;
    QString m_settingsId;
    QLabel* m_sizeLabel;
    QPointer<QLayout> m_footer; ///< 文字サイズ操作と決定ボタンを並べる操作列（二段に折り返す）
    QPointer<QLayout> m_hostLayout; ///< 操作列を入れたダイアログのレイアウト（QMessageBox は作り直すことがある）
    int m_size;
    int m_baseSize;
    bool m_saveSize;
    bool m_attached = false;
};

#endif // DIALOGFONTSCALE_H
