#ifndef JOSEKIMOVEDIALOG_H
#define JOSEKIMOVEDIALOG_H

/// @file josekimovedialog.h
/// @brief 定跡手入力ダイアログクラスの定義


#include <QDialog>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QRadioButton>
#include <QButtonGroup>
#include <QLabel>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QGroupBox>

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
    /**
     * @brief 入力値を検証する
     */
    void validateInput();
    
    /**
     * @brief 指し手の入力方式が変更された時
     */
    void onMoveInputModeChanged();
    
    /**
     * @brief 指し手のコンボボックスが変更された時
     */
    void onMoveComboChanged();
    
    /**
     * @brief 予想応手の入力方式が変更された時
     */
    void onNextMoveInputModeChanged();
    
    /**
     * @brief 予想応手のコンボボックスが変更された時
     */
    void onNextMoveComboChanged();
    
    /**
     * @brief フォントサイズを拡大する
     */
    void onFontSizeIncrease();
    
    /**
     * @brief フォントサイズを縮小する
     */
    void onFontSizeDecrease();

private:
    /**
     * @brief UIをセットアップする
     * @param isEdit true=編集モード
     */
    void setupUi(bool isEdit);
    
    /**
     * @brief 指し手入力ウィジェットを作成
     * @param parent 親ウィジェット
     * @param isNextMove true=予想応手用, false=指し手用
     * @return 作成したグループボックス
     */
    QGroupBox* createMoveInputWidget(QWidget *parent, bool isNextMove);
    
    /**
     * @brief USI形式の指し手が有効かどうかを検証
     * @param move 指し手文字列
     * @return 有効な場合true
     */
    bool isValidUsiMove(const QString &move) const;
    
    /**
     * @brief コンボボックスの選択からUSI形式の指し手を生成
     * @param isDrop 駒打ちかどうか
     * @param pieceCombo 駒種コンボボックス
     * @param fromFileCombo 移動元筋コンボボックス
     * @param fromRankCombo 移動元段コンボボックス
     * @param toFileCombo 移動先筋コンボボックス
     * @param toRankCombo 移動先段コンボボックス
     * @param promoteCombo 成りコンボボックス
     * @return USI形式の指し手
     */
    QString generateUsiMove(bool isDrop,
                            QComboBox *pieceCombo,
                            QComboBox *fromFileCombo,
                            QComboBox *fromRankCombo,
                            QComboBox *toFileCombo,
                            QComboBox *toRankCombo,
                            QComboBox *promoteCombo) const;
    
    /**
     * @brief USI形式の指し手を日本語表記に変換
     * @param usiMove USI形式の指し手
     * @return 日本語表記
     */
    QString usiToJapanese(const QString &usiMove) const;
    
    /**
     * @brief USI形式の指し手からコンボボックスを設定
     * @param usiMove USI形式の指し手
     * @param isNextMove true=予想応手用
     */
    void setComboFromUsi(const QString &usiMove, bool isNextMove);
    
    /**
     * @brief 指し手プレビューを更新
     */
    void updateMovePreview();
    
    /**
     * @brief 予想応手プレビューを更新
     */
    void updateNextMovePreview();
    
    /**
     * @brief フォントサイズを適用する
     */
    void applyFontSize();

    // === 指し手入力 ===
    QRadioButton *m_moveBoardRadio = nullptr;      ///< 盤上の駒を動かすラジオボタン
    QRadioButton *m_moveDropRadio = nullptr;       ///< 持ち駒を打つラジオボタン
    QButtonGroup *m_moveTypeGroup = nullptr;       ///< 指し手種別グループ

    QComboBox *m_moveFromFileCombo = nullptr;      ///< 移動元筋
    QComboBox *m_moveFromRankCombo = nullptr;      ///< 移動元段
    QComboBox *m_moveToFileCombo = nullptr;        ///< 移動先筋
    QComboBox *m_moveToRankCombo = nullptr;        ///< 移動先段
    QComboBox *m_moveDropPieceCombo = nullptr;     ///< 打つ駒種
    QComboBox *m_moveDropToFileCombo = nullptr;    ///< 駒打ち先筋
    QComboBox *m_moveDropToRankCombo = nullptr;    ///< 駒打ち先段
    QComboBox *m_movePromoteCombo = nullptr;       ///< 成り/不成

    QLabel *m_movePreviewLabel = nullptr;          ///< 指し手プレビュー（日本語）
    QLabel *m_moveUsiLabel = nullptr;              ///< 指し手USI形式表示

    QWidget *m_moveBoardWidget = nullptr;          ///< 盤上移動用ウィジェット
    QWidget *m_moveDropWidget = nullptr;           ///< 駒打ち用ウィジェット

    // === 予想応手入力 ===
    QRadioButton *m_nextMoveBoardRadio = nullptr;  ///< 盤上の駒を動かすラジオボタン
    QRadioButton *m_nextMoveDropRadio = nullptr;   ///< 持ち駒を打つラジオボタン
    QRadioButton *m_nextMoveNoneRadio = nullptr;   ///< 予想応手なしラジオボタン
    QButtonGroup *m_nextMoveTypeGroup = nullptr;   ///< 予想応手種別グループ

    QComboBox *m_nextMoveFromFileCombo = nullptr;  ///< 移動元筋
    QComboBox *m_nextMoveFromRankCombo = nullptr;  ///< 移動元段
    QComboBox *m_nextMoveToFileCombo = nullptr;    ///< 移動先筋
    QComboBox *m_nextMoveToRankCombo = nullptr;    ///< 移動先段
    QComboBox *m_nextMoveDropPieceCombo = nullptr;  ///< 打つ駒種
    QComboBox *m_nextMoveDropToFileCombo = nullptr; ///< 駒打ち先筋
    QComboBox *m_nextMoveDropToRankCombo = nullptr; ///< 駒打ち先段
    QComboBox *m_nextMovePromoteCombo = nullptr;   ///< 成り/不成

    QLabel *m_nextMovePreviewLabel = nullptr;      ///< 予想応手プレビュー（日本語）
    QLabel *m_nextMoveUsiLabel = nullptr;          ///< 予想応手USI形式表示

    QWidget *m_nextMoveBoardWidget = nullptr;      ///< 盤上移動用ウィジェット
    QWidget *m_nextMoveDropWidget = nullptr;       ///< 駒打ち用ウィジェット
    QWidget *m_nextMoveInputWidget = nullptr;      ///< 予想応手入力エリア全体

    // === 編集モード用 ===
    QLabel    *m_editMoveLabel = nullptr;          ///< 編集対象の定跡手表示ラベル

    // === 評価情報 ===
    QSpinBox  *m_valueSpinBox = nullptr;           ///< 評価値入力欄
    QSpinBox  *m_depthSpinBox = nullptr;           ///< 深さ入力欄
    QSpinBox  *m_frequencySpinBox = nullptr;       ///< 出現頻度入力欄
    QLineEdit *m_commentEdit = nullptr;            ///< コメント入力欄

    // === フォントサイズ ===
    QPushButton *m_fontIncreaseBtn = nullptr;      ///< フォント拡大ボタン
    QPushButton *m_fontDecreaseBtn = nullptr;      ///< フォント縮小ボタン
    int m_fontSize = 0;                            ///< 現在のフォントサイズ

    QLabel    *m_moveErrorLabel = nullptr;         ///< 指し手エラー表示

    QDialogButtonBox *m_buttonBox = nullptr;       ///< OK/キャンセルボタン
};

#endif // JOSEKIMOVEDIALOG_H
