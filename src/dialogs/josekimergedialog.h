#ifndef JOSEKIMERGEDIALOG_H
#define JOSEKIMERGEDIALOG_H

/// @file josekimergedialog.h
/// @brief 棋譜の手を選んで定跡に登録するダイアログ

#include <QDialog>
#include <QList>
#include <QString>

#include "recordline.h"

class JosekiBookSession;
class QButtonGroup;
class QLabel;
class QPushButton;
class QTableWidget;

/**
 * @brief 棋譜の手順（表示中の手順か棋譜ファイルの本譜）の手を、定跡に1手ずつか全部登録するダイアログ
 *
 * 行は手順の手（手数・表記）で、定跡に同じ局面・同じ指し手があれば「登録済」にして登録させない（定跡の中身で決める）。
 * 登録済みでない手を JosekiBookSession::importMoves で出現頻度1で足し、保存はしない
 * （定跡ウィンドウのほかの編集と同じく、定跡ウィンドウの「保存」で保存する）。
 * 登録先はいつも開いている定跡で、定跡が変わると表を作り直し、開いている定跡ファイルが替わると登録先の表示を替える。
 * 定跡を変えられないとき（読み書き中・前回のファイルの読み込み待ち）は使えなくする。
 */
class JosekiMergeDialog : public QDialog
{
    Q_OBJECT

public:
    /// line の手を session の定跡に登録する（line.currentPly の手に印を付ける）
    JosekiMergeDialog(JosekiBookSession *session, const RecordLine &line, QWidget *parent = nullptr);
    ~JosekiMergeDialog() override;

private slots:
    /// 行の「登録」ボタン
    void onRegisterButtonClicked(int row);
    void onRegisterAllButtonClicked();
    /// 定跡が変わった（登録済みの印を作り直す）
    void updateTable();
    /// 読み書き中・前回のファイルの読み込み待ちは使えなくする
    void updateEnabled();
    /// 登録先（開いている定跡ファイル）の表示
    void updateTargetLabel();
    /// 文字の大きさに合わせて行の高さと列の幅を決め直す
    void fitRowsToFont();

private:
    /// 手順の1手
    struct Row {
        int ply = 0;
        QString sfen;      ///< 指す前の局面
        QString usiMove;
        QString moveText;  ///< 棋譜の表記
    };

    void setupUi();
    bool isRegistered(const Row &row) const;

    JosekiBookSession *m_session = nullptr;
    QList<Row> m_rows;
    int m_currentPly = 0;

    QTableWidget *m_tableWidget = nullptr;
    QPushButton *m_registerAllButton = nullptr;
    QPushButton *m_closeButton = nullptr;
    QPushButton *m_fontIncreaseBtn = nullptr;
    QPushButton *m_fontDecreaseBtn = nullptr;
    QLabel *m_statusLabel = nullptr;
    QLabel *m_targetFileLabel = nullptr;
    QButtonGroup *m_registerButtons = nullptr;  ///< 行の「登録」ボタン（id が行）
};

#endif // JOSEKIMERGEDIALOG_H
