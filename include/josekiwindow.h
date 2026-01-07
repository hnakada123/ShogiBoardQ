#ifndef JOSEKIWINDOW_H
#define JOSEKIWINDOW_H

#include <QWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QLabel>
#include <QTableWidget>
#include <QMap>
#include <QVector>
#include <QCloseEvent>

/**
 * @brief 定跡エントリの指し手情報
 */
struct JosekiMove {
    QString move;       ///< 指し手（USI形式）
    QString nextMove;   ///< 次の指し手（USI形式）
    int value;          ///< 評価値
    int depth;          ///< 深さ
    int frequency;      ///< 出現頻度
    QString comment;    ///< コメント
};

/**
 * @brief 定跡エントリ（局面ごとの定跡データ）
 */
struct JosekiEntry {
    QString sfen;                   ///< 局面のSFEN文字列
    QVector<JosekiMove> moves;      ///< この局面での指し手リスト
};

/**
 * @brief 定跡ウィンドウクラス
 *
 * 定跡データベースの表示・編集を行うウィンドウ。
 * メインウィンドウとは独立したウィンドウとして表示される。
 */
class JosekiWindow : public QWidget
{
    Q_OBJECT

public:
    explicit JosekiWindow(QWidget *parent = nullptr);
    ~JosekiWindow() override = default;

    /**
     * @brief 現在の局面のSFENを設定し、定跡を検索・表示する
     * @param sfen 局面のSFEN文字列
     */
    void setCurrentSfen(const QString &sfen);

public slots:
    /**
     * @brief 「開く」ボタンがクリックされたときのスロット
     */
    void onOpenButtonClicked();
    
    /**
     * @brief フォントサイズを拡大する
     */
    void onFontSizeIncrease();
    
    /**
     * @brief フォントサイズを縮小する
     */
    void onFontSizeDecrease();

protected:
    /**
     * @brief ウィンドウが閉じられる時に設定を保存
     */
    void closeEvent(QCloseEvent *event) override;

private:
    /**
     * @brief UIコンポーネントのセットアップ
     */
    void setupUi();

    /**
     * @brief 定跡ファイルを読み込む
     * @param filePath ファイルパス
     * @return 読み込み成功時true
     */
    bool loadJosekiFile(const QString &filePath);

    /**
     * @brief 定跡ファイルの1行をパースする
     * @param sfenLine sfen行
     * @param moveLine 指し手行
     * @return パースしたエントリ
     */
    JosekiEntry parseJosekiEntry(const QString &sfenLine, const QString &moveLine);

    /**
     * @brief 現在の局面に一致する定跡を表示する
     */
    void updateJosekiDisplay();

    /**
     * @brief 表をクリアしてヘッダーのみ表示する
     */
    void clearTable();

    /**
     * @brief SFEN文字列を正規化する（手数部分を除く）
     * @param sfen SFEN文字列
     * @return 正規化されたSFEN文字列
     */
    QString normalizeSfen(const QString &sfen) const;
    
    /**
     * @brief フォントサイズを適用する
     */
    void applyFontSize();
    
    /**
     * @brief 設定を読み込む
     */
    void loadSettings();
    
    /**
     * @brief 設定を保存する
     */
    void saveSettings();

    QPushButton  *m_openButton;        ///< 「開く」ボタン
    QLabel       *m_filePathLabel;     ///< 選択されたファイルパスを表示するラベル
    QLabel       *m_currentSfenLabel;  ///< 現在の局面のSFEN表示用ラベル（デバッグ用）
    QPushButton  *m_fontIncreaseBtn;   ///< フォント拡大ボタン
    QPushButton  *m_fontDecreaseBtn;   ///< フォント縮小ボタン
    QTableWidget *m_tableWidget;       ///< 定跡表示用テーブル
    QString       m_currentFilePath;   ///< 現在選択されているファイルパス
    QString       m_currentSfen;       ///< 現在の局面のSFEN
    int           m_fontSize;          ///< 現在のフォントサイズ

    /// 定跡データ（SFEN → エントリリスト）
    QMap<QString, QVector<JosekiMove>> m_josekiData;
};

#endif // JOSEKIWINDOW_H
