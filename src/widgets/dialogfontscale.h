#ifndef DIALOGFONTSCALE_H
#define DIALOGFONTSCALE_H

#include <QPointer>
#include <QSize>
#include <QWidget>

#include "setting.h"

class FontSizeControl;
class FontSizeHelper;
class QAbstractButton;
class QCoreApplication;
class QDialog;
class QLabel;
class QLayout;

/**
 * @brief ダイアログの文字の大きさ（A-/A+）と保存を受け持つ（全ダイアログ共通の仕組み）
 *
 * 各部品の元の大きさからの差で拡大・縮小するので、見出しなどの相対的な大きさは保たれる。
 * A-/A+ はダイアログが置いたボタンを渡せばそれを使い、渡さなければ決定ボタンの列に作る。
 * 独自描画の盤面などは dialogFontScaleExcluded プロパティで対象外にできる。
 */
class DialogFontScale : public QWidget
{
public:
    struct Options {
        const Setting<QSize>* size = nullptr;   ///< ダイアログの大きさも保存する
        int minSize = 8;
        int maxSize = 24;
        QAbstractButton* decrease = nullptr;    ///< ダイアログが置いた A-（無ければ作る）
        QAbstractButton* increase = nullptr;    ///< ダイアログが置いた A+
        QLabel* sizeLabel = nullptr;            ///< 今の大きさを出すラベル（ボタンを渡したとき）
    };

    /**
     * @brief dialog に文字の大きさの操作を付ける
     * @param fontSize 保存先。既定値が 0 なら、未保存のときはダイアログの元の大きさを使う
     * @return 付けた部品（fontSize()->fontSizeChanged で、大きさに合わせた追加の調整ができる）
     */
    static DialogFontScale* install(QDialog* dialog, const Setting<int>& fontSize, const Options& options);
    static DialogFontScale* install(QDialog* dialog, const Setting<int>& fontSize) { return install(dialog, fontSize, {}); }

    /// 文字の大きさ（範囲と保存）
    FontSizeHelper* fontSize() const { return m_fontSize; }
    /// 後から作った部品にも今の大きさを付ける（内容を作り直したとき）
    void rescale();

    /// 以後に表示するすべての QMessageBox（静的関数で出すものを含む）に文字サイズ操作を付ける。
    /// 文字サイズはメッセージボックス全体で共有して保存する。
    static void installForMessageBoxes(QCoreApplication* app);

    /// installForMessageBoxes で付けた操作が、表示時にメッセージボックスへ適用する文字（保存済みの大きさ）を返す。
    /// 表示前に本文の幅を測るときに使う。
    static QFont messageBoxFont(const QFont& font);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    DialogFontScale(QDialog* dialog, const Setting<int>& fontSize, const Options& options);
    void applySize(int size);
    void updateLayout();
    bool attachToLayout();
    void ensureContentFits();

    QDialog* m_dialog;
    QLabel* m_sizeLabel = nullptr;
    FontSizeHelper* m_fontSize;  ///< 文字の大きさ（範囲と保存。A-/A+ をつなぐ）
    FontSizeControl* m_buttons = nullptr;   ///< A-/A+（m_fontSize の子）
    QPointer<QLayout> m_footer; ///< 文字サイズ操作と決定ボタンを並べる操作列（二段に折り返す）
    QPointer<QLayout> m_hostLayout; ///< 操作列を入れたダイアログのレイアウト（QMessageBox は作り直すことがある）
    int m_baseSize;
    const Setting<QSize>* m_sizeSetting = nullptr;
    bool m_ownButtons = true;   ///< A-/A+ をこの部品が作って操作列に置いた
    bool m_attached = false;
};

#endif // DIALOGFONTSCALE_H
