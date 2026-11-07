#ifndef JOSEKIMOVEDIALOG_H
#define JOSEKIMOVEDIALOG_H

/// @file josekimovedialog.h
/// @brief 定跡手入力ダイアログクラスの定義

#include <QDialog>

#include "josekitypes.h"

class ElideLabel;
class JosekiMoveBoard;
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
 * 指し手は盤で指すか、局面の合法手の一覧から選ぶ（JosekiMoveBoard・JosekiMoveInputWidget）。
 * 盤で指した手は一覧に入れ、盤は一覧で選ばれた手から局面を決めるので、選んだ手の正本は一覧だけ。
 * 値の受け渡しは setEntry()/entry()。
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

    /// 登録対象の局面（指し手はこの局面の手、予想応手は着手後の局面の手を盤で指すか一覧から選ぶ）
    void setCurrentSfen(const QString &sfen);

    /**
     * @brief 定跡手の値を入力欄に入れる
     *
     * 編集モードでは指し手は変えられず、entry() は渡した指し手をそのまま返す。
     * 指し手・予想応手が局面の合法手でなければ選んでいない状態になり、「追加」「更新」は押せない。
     */
    void setEntry(const JosekiMove &entry);
    /// 入力した定跡手（予想応手が「なし」なら nextMove は nullopt）
    JosekiMove entry() const;

    /**
     * @brief 編集対象の定跡手を出す（編集モード用）
     * @param moveText 画面の表記の定跡手（JosekiTableModel::moveText。表記の設定に従う）
     */
    void setEditMoveDisplay(const QString &moveText);

public slots:
    void done(int result) override;

private slots:
    void validateInput();
    void updateValidation();
    /// 盤で手を指した（まだ指し手が無ければ指し手、あれば予想応手にする）
    void onBoardMovePlayed(const QString &usiMove);
    /// 指し手を指し直すときは、その後の予想応手も取り消す
    void clearNextMove();

private:
    void setupUi(bool isEdit);
    QString inputError() const;
    /// 盤と案内を、選んだ手に合わせる。after は指し手の後の局面（指し手が決まっていなければ空）
    void updateBoard(const QString &after);
    /// 指し手（編集モードは渡された指し手、追加モードは選んだ指し手）
    QString move() const;

    // 盤と、次に何を指すかの案内
    JosekiMoveBoard *m_board = nullptr;
    ElideLabel *m_guideLabel = nullptr;

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

    QLabel *m_moveErrorLabel = nullptr;
    QLabel *m_positionLabel = nullptr;
    QString m_currentSfen;
    bool m_isEdit = false;
    QString m_editMove;   ///< 編集モードの指し手（変えられない）
    QDialogButtonBox *m_buttonBox = nullptr;
};

#endif // JOSEKIMOVEDIALOG_H
