#ifndef JOSEKIMOVEINPUTWIDGET_H
#define JOSEKIMOVEINPUTWIDGET_H

/// @file josekimoveinputwidget.h
/// @brief 定跡手入力ウィジェットクラスの定義

#include <QGroupBox>

class QButtonGroup;
class QComboBox;
class QLabel;
class QRadioButton;

/**
 * @brief 定跡手入力ウィジェット
 *
 * 盤上移動・駒打ちをコンボボックスで入力し、USI形式の指し手を生成する。
 * 「指し手」と「予想応手」の両方に再利用可能。
 */
class JosekiMoveInputWidget : public QGroupBox
{
    Q_OBJECT

public:
    /**
     * @brief コンストラクタ
     * @param title グループボックスのタイトル
     * @param hasNoneOption 「なし」ラジオボタンを表示するか
     * @param parent 親ウィジェット
     */
    explicit JosekiMoveInputWidget(const QString &title, bool hasNoneOption, QWidget *parent = nullptr);

    /**
     * @brief USI形式の指し手を取得
     * @return USI形式の指し手（「なし」選択時は "none"）
     */
    QString usiMove() const;

    /**
     * @brief USI形式の指し手を設定
     * @param usiMove USI形式の指し手（"none" で「なし」を選択）
     */
    void setUsiMove(const QString &usiMove);

    /**
     * @brief USI形式の指し手が有効かどうかを検証
     * @param move 指し手文字列
     * @return 有効な場合true
     */
    static bool isValidUsiMove(const QString &move);

    /**
     * @brief フォントを適用する
     * @param baseFont ベースフォント
     */
    void applyFont(const QFont &baseFont);

signals:
    /**
     * @brief 指し手が変更された
     */
    void moveChanged();

private slots:
    void onInputModeChanged();
    void onComboChanged();

private:
    void setupUi(bool hasNoneOption);
    void updatePreview();
    QString buildUsiMove() const;
    void setComboFromUsi(const QString &usiMove);

    static QString usiToJapanese(const QString &usiMove);

    // ラジオボタン
    QRadioButton *m_boardRadio = nullptr;
    QRadioButton *m_dropRadio = nullptr;
    QRadioButton *m_noneRadio = nullptr;
    QButtonGroup *m_typeGroup = nullptr;

    // 盤上移動用コンボボックス
    QComboBox *m_fromFileCombo = nullptr;
    QComboBox *m_fromRankCombo = nullptr;
    QComboBox *m_toFileCombo = nullptr;
    QComboBox *m_toRankCombo = nullptr;
    QComboBox *m_promoteCombo = nullptr;

    // 駒打ち用コンボボックス
    QComboBox *m_dropPieceCombo = nullptr;
    QComboBox *m_dropToFileCombo = nullptr;
    QComboBox *m_dropToRankCombo = nullptr;

    // プレビュー
    QLabel *m_previewLabel = nullptr;
    QLabel *m_usiLabel = nullptr;

    // 入力エリアウィジェット
    QWidget *m_boardWidget = nullptr;
    QWidget *m_dropWidget = nullptr;
    QWidget *m_inputWidget = nullptr;
};

#endif // JOSEKIMOVEINPUTWIDGET_H
