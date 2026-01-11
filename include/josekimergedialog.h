#ifndef JOSEKIMERGEDIALOG_H
#define JOSEKIMERGEDIALOG_H

#include <QDialog>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QVector>
#include <QString>
#include <QSet>

/**
 * @brief 棋譜からの定跡マージ用エントリ
 */
struct KifuMergeEntry {
    int ply;              ///< 手数
    QString sfen;         ///< この手を指す前の局面SFEN
    QString usiMove;      ///< USI形式の指し手
    QString japaneseMove; ///< 日本語表記の指し手
    bool isCurrentMove;   ///< 現在選択中の指し手かどうか
};

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
    ~JosekiMergeDialog() override = default;

    /**
     * @brief 棋譜データを設定する
     * @param entries 棋譜エントリのリスト
     * @param currentPly 現在選択中の手数
     */
    void setKifuData(const QVector<KifuMergeEntry> &entries, int currentPly);
    
    /**
     * @brief マージ先の定跡ファイル名を設定する
     * @param filePath 定跡ファイルのパス
     */
    void setTargetJosekiFile(const QString &filePath);
    
    /**
     * @brief 登録済みの指し手セットを設定する
     * @param registeredMoves 登録済みの「正規化SFEN:USI指し手」のセット
     */
    void setRegisteredMoves(const QSet<QString> &registeredMoves);

signals:
    /**
     * @brief 指し手を定跡に登録するシグナル
     * @param sfen 局面SFEN（手数を除いた正規化形式）
     * @param sfenWithPly 局面SFEN（手数付き）
     * @param usiMove USI形式の指し手
     */
    void registerMove(const QString &sfen, const QString &sfenWithPly, const QString &usiMove);
    
    /**
     * @brief 全ての指し手を一括登録するシグナル
     * @param entries 登録する棋譜エントリのリスト
     */
    void registerAllMoves(const QVector<KifuMergeEntry> &entries);

private slots:
    /**
     * @brief 「登録」ボタンがクリックされた時のスロット
     */
    void onRegisterButtonClicked();
    
    /**
     * @brief 「全て登録」ボタンがクリックされた時のスロット
     */
    void onRegisterAllButtonClicked();
    
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
    void applyFontSize();
    
    /**
     * @brief SFENを正規化（手数を除去）
     * @param sfen 元のSFEN
     * @return 正規化されたSFEN
     */
    QString normalizeSfen(const QString &sfen) const;
    
    /**
     * @brief 登録済みかどうかを確認する
     * @param sfen 局面SFEN
     * @param usiMove USI形式の指し手
     * @return 登録済みならtrue
     */
    bool isRegistered(const QString &sfen, const QString &usiMove) const;

    QTableWidget *m_tableWidget;       ///< 棋譜表示テーブル
    QPushButton *m_registerAllButton;  ///< 全て登録ボタン
    QPushButton *m_closeButton;        ///< 閉じるボタン
    QPushButton *m_fontIncreaseBtn;    ///< フォント拡大ボタン
    QPushButton *m_fontDecreaseBtn;    ///< フォント縮小ボタン
    QLabel *m_statusLabel;             ///< 状態ラベル
    QLabel *m_targetFileLabel;         ///< マージ先ファイルラベル
    QLabel *m_autoSaveLabel;           ///< 自動保存説明ラベル
    
    QVector<KifuMergeEntry> m_entries; ///< 棋譜エントリ
    QSet<QString> m_registeredMoves;   ///< 登録済みの指し手セット（「正規化SFEN:USI指し手」形式）
    int m_currentPly;                  ///< 現在選択中の手数
    int m_fontSize;                    ///< フォントサイズ
};

#endif // JOSEKIMERGEDIALOG_H
