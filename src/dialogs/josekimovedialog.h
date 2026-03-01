#ifndef JOSEKIMOVEDIALOG_H
#define JOSEKIMOVEDIALOG_H

/// @file josekimovedialog.h
/// @brief 定跡手入力ダイアログクラスの定義

#include <QDialog>

class JosekiMoveInputWidget;
class QDialogButtonBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;

/**
 * @brief 定跡手追加・編集ダイアログ
 *
 * やねうら王定跡フォーマットに基づいた定跡手の追加・編集を行うダイアログ。
 * フォーマット: 指し手 予想応手 評価値 深さ 出現頻度 [コメント]
 *
 * コンボボックスによる直感的な入力と、USI形式のプレビュー表示をサポート。
 */
class JosekiMoveDialog : public QDialog
{
    Q_OBJECT

public:
    /**
     * @brief コンストラクタ
     * @param parent 親ウィジェット
     * @param isEdit true=編集モード, false=追加モード
     */
    explicit JosekiMoveDialog(QWidget *parent = nullptr, bool isEdit = false);
    ~JosekiMoveDialog() override = default;

    /**
     * @brief 指し手を取得
     * @return USI形式の指し手
     */
    QString move() const;

    /**
     * @brief 指し手を設定
     * @param move USI形式の指し手
     */
    void setMove(const QString &move);

    /**
     * @brief 予想応手を取得
     * @return USI形式の予想応手（なければ"none"）
     */
    QString nextMove() const;

    /**
     * @brief 予想応手を設定
     * @param nextMove USI形式の予想応手
     */
    void setNextMove(const QString &nextMove);

    /**
     * @brief 評価値を取得
     * @return 評価値
     */
    int value() const;

    /**
     * @brief 評価値を設定
     * @param value 評価値
     */
    void setValue(int value);

    /**
     * @brief 深さを取得
     * @return 探索深さ
     */
    int depth() const;

    /**
     * @brief 深さを設定
     * @param depth 探索深さ
     */
    void setDepth(int depth);

    /**
     * @brief 出現頻度を取得
     * @return 出現頻度
     */
    int frequency() const;

    /**
     * @brief 出現頻度を設定
     * @param frequency 出現頻度
     */
    void setFrequency(int frequency);

    /**
     * @brief コメントを取得
     * @return コメント
     */
    QString comment() const;

    /**
     * @brief コメントを設定
     * @param comment コメント
     */
    void setComment(const QString &comment);

    /**
     * @brief 編集対象の定跡手を日本語で表示（編集モード用）
     * @param japaneseMove 日本語表記の定跡手（例：▲７六歩(77)）
     */
    void setEditMoveDisplay(const QString &japaneseMove);

private slots:
    void validateInput();
    void onFontSizeIncrease();
    void onFontSizeDecrease();

private:
    void setupUi(bool isEdit);
    void applyFontSize();

    // 指し手入力ウィジェット
    JosekiMoveInputWidget *m_moveInput = nullptr;
    JosekiMoveInputWidget *m_nextMoveInput = nullptr;

    // 編集モード用
    QLabel *m_editMoveLabel = nullptr;

    // 評価情報
    QSpinBox *m_valueSpinBox = nullptr;
    QSpinBox *m_depthSpinBox = nullptr;
    QSpinBox *m_frequencySpinBox = nullptr;
    QLineEdit *m_commentEdit = nullptr;

    // フォントサイズ
    QPushButton *m_fontIncreaseBtn = nullptr;
    QPushButton *m_fontDecreaseBtn = nullptr;
    int m_fontSize = 0;

    QLabel *m_moveErrorLabel = nullptr;
    QDialogButtonBox *m_buttonBox = nullptr;
};

#endif // JOSEKIMOVEDIALOG_H
