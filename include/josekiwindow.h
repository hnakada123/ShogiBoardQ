#ifndef JOSEKIWINDOW_H
#define JOSEKIWINDOW_H

#include <QWidget>
#include <QPushButton>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QLabel>
#include <QTableWidget>
#include <QMap>
#include <QVector>
#include <QCloseEvent>

// 前方宣言
class SfenPositionTracer;

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
    
    /**
     * @brief 人間が着手可能かどうかを設定する
     * @param canPlay true=人間の手番で着手可能、false=エンジンの手番で着手不可
     */
    void setHumanCanPlay(bool canPlay);

signals:
    /**
     * @brief 定跡手が選択されたときに発行されるシグナル
     * @param usiMove USI形式の指し手（例："7g7f", "P*5e"）
     */
    void josekiMoveSelected(const QString &usiMove);

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
    
    /**
     * @brief 定跡手の着手結果を受け取るスロット
     * @param success 着手が成功したかどうか
     * @param usiMove 着手しようとした指し手
     */
    void onMoveResult(bool success, const QString &usiMove);

private slots:
    /**
     * @brief 「着手」ボタンがクリックされたときのスロット
     */
    void onPlayButtonClicked();
    
    /**
     * @brief 自動読込チェックボックスの状態が変更されたときのスロット
     */
    void onAutoLoadCheckBoxChanged(Qt::CheckState state);
    
    /**
     * @brief 「停止」ボタンがクリックされたときのスロット
     */
    void onStopButtonClicked();
    
    /**
     * @brief 「閉じる」ボタンがクリックされたときのスロット
     */
    void onCloseButtonClicked();

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
    
    /**
     * @brief USI形式の指し手を日本語表記に変換する
     * @param usiMove USI形式の指し手（例："7g7f"）
     * @param plyNumber 手数（奇数=先手、偶数=後手）
     * @param tracer 現在局面をセットしたSfenPositionTracer
     * @return 日本語表記（例："▲７六歩(77)"）
     */
    QString usiMoveToJapanese(const QString &usiMove, int plyNumber, SfenPositionTracer &tracer) const;
    
    /**
     * @brief 駒の日本語名を取得
     * @param pieceChar 駒文字（P, L, N, S, G, B, R, K）
     * @param promoted 成駒かどうか
     * @return 日本語名（歩, 香, 桂, 銀, 金, 角, 飛, 玉, と, 杏, 圭, 全, 馬, 龍）
     */
    static QString pieceToKanji(QChar pieceChar, bool promoted = false);

    QPushButton  *m_openButton;        ///< 「開く」ボタン
    QLabel       *m_filePathLabel;     ///< 選択されたファイルパスを表示するラベル
    QLabel       *m_currentSfenLabel;  ///< 現在の局面のSFEN表示用ラベル（デバッグ用）
    QPushButton  *m_fontIncreaseBtn;   ///< フォント拡大ボタン
    QPushButton  *m_fontDecreaseBtn;   ///< フォント縮小ボタン
    QCheckBox    *m_autoLoadCheckBox;  ///< 自動読込チェックボックス
    QPushButton  *m_stopButton;        ///< 定跡表示停止ボタン
    QPushButton  *m_closeButton;       ///< 閉じるボタン
    QTableWidget *m_tableWidget;       ///< 定跡表示用テーブル
    QString       m_currentFilePath;   ///< 現在選択されているファイルパス
    QString       m_currentSfen;       ///< 現在の局面のSFEN
    int           m_fontSize;          ///< 現在のフォントサイズ
    bool          m_humanCanPlay;      ///< 人間が着手可能かどうか
    bool          m_autoLoadEnabled;   ///< 定跡ファイル自動読込が有効かどうか
    bool          m_displayEnabled;    ///< 定跡表示が有効かどうか
    
    /// 現在表示中の定跡手リスト（着手ボタンから参照）
    QVector<JosekiMove> m_currentMoves;

    /// 定跡データ（SFEN → エントリリスト）
    QMap<QString, QVector<JosekiMove>> m_josekiData;
};

#endif // JOSEKIWINDOW_H
