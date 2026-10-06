#ifndef DIALOGFONTSCALE_H
#define DIALOGFONTSCALE_H

#include <QWidget>

class QDialog;
class QLabel;
class QLayout;

/// 補助ダイアログに文字サイズ操作と保存を追加する。
/// 独自描画の盤面などは dialogFontScaleExcluded プロパティで対象外にできる。
class DialogFontScale : public QWidget
{
public:
    static void install(QDialog* dialog, const QString& settingsId, bool saveSize = false);

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
    QLayout* m_footer = nullptr; ///< 文字サイズ操作と決定ボタンを並べる操作列（二段に折り返す）
    int m_size;
    int m_baseSize;
    bool m_saveSize;
    bool m_attached = false;
};

#endif // DIALOGFONTSCALE_H
