#ifndef JOSEKIMERGEDIALOG_H
#define JOSEKIMERGEDIALOG_H

/// @file josekimergedialog.h
/// @brief 定跡統合ダイアログクラスの定義

#include <QDialog>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QList>
#include <QString>
#include <QSet>
#include <QButtonGroup>
#include <QCloseEvent>
#include <functional>

#include "josekitypes.h"

/**
 * @brief 定跡マージダイアログクラス
 * 
 * 現在の棋譜から指し手を選択して定跡にマージするためのダイアログ
 */
class JosekiMergeDialog : public QDialog
{
    Q_OBJECT

public:
    /**
     * @brief コンストラクタ
     * @param parent 親ウィジェット
     */
    explicit JosekiMergeDialog(QWidget *parent = nullptr);
    ~JosekiMergeDialog() override;

    /**
     * @brief 棋譜データを設定する
     * @param entries 棋譜エントリのリスト
     */
    void setKifuData(const QList<KifuMergeEntry> &entries);
    
    /**
     * @brief マージ先の定跡ファイル名を設定する
     * @param filePath 定跡ファイルのパス
     */
    void setTargetJosekiFile(const QString &filePath);
    
    /**
     * @brief 登録済みの指し手セットを設定する
     * @param registeredMoves 登録済みの指し手（局面のキーと USI の指し手の組）
     */
    void setRegisteredMoves(const QSet<JosekiMoveKey> &registeredMoves);

    /// 指し手を定跡に登録する関数（登録して定跡ファイルに保存できたら true）
    using Registrar = std::function<bool(const QList<KifuMergeEntry> &entries)>;
    /// 登録する関数を設定する。空にすると登録できなくなる（定跡ファイルを替えたとき）
    void setRegistrar(Registrar registrar);

private slots:
    /// 行の「登録」ボタン
    void onRegisterButtonClicked(int row);
    
    /**
     * @brief 「全て登録」ボタンがクリックされた時のスロット
     */
    void onRegisterAllButtonClicked();
    
    

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    /**
     * @brief UIを構築する
     */
    void setupUi();
    
    /**
     * @brief テーブルを更新する
     */
    void updateTable();
    
    /**
     * @brief フォントサイズを適用する
     */
    /// 文字の大きさに合わせて行の高さと列の幅を決め直す
    void fitRowsToFont();
    /// 指し手を登録する（登録して保存できたら true）
    bool registerEntries(const QList<KifuMergeEntry> &entries);
    
    /// 指し手の見分け方（局面のキー・USI の指し手。JosekiBook と同じ）
    static JosekiMoveKey moveKey(const KifuMergeEntry &entry);
    
    /**
     * @brief 登録済みかどうかを確認する
     * @param sfen 局面SFEN
     * @param usiMove USI形式の指し手
     * @return 登録済みならtrue
     */
    bool isRegistered(const QString &sfen, const QString &usiMove) const;

    QTableWidget *m_tableWidget = nullptr;       ///< 棋譜表示テーブル
    QPushButton *m_registerAllButton = nullptr;  ///< 全て登録ボタン
    QPushButton *m_closeButton = nullptr;        ///< 閉じるボタン
    QPushButton *m_fontIncreaseBtn = nullptr;    ///< フォント拡大ボタン
    QPushButton *m_fontDecreaseBtn = nullptr;    ///< フォント縮小ボタン
    QLabel *m_statusLabel = nullptr;             ///< 状態ラベル
    QLabel *m_targetFileLabel = nullptr;         ///< マージ先ファイルラベル
    QLabel *m_autoSaveLabel = nullptr;           ///< 自動保存説明ラベル

    QList<KifuMergeEntry> m_entries; ///< 棋譜エントリ
    QSet<JosekiMoveKey> m_registeredMoves; ///< 登録済みの指し手（局面のキーと USI の指し手の組）
    Registrar m_registrar;
    QButtonGroup *m_registerButtons = nullptr;  ///< 行の「登録」ボタン（id が行）
};

#endif // JOSEKIMERGEDIALOG_H
