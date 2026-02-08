#ifndef JOSEKIWINDOW_H
#define JOSEKIWINDOW_H

/// @file josekiwindow.h
/// @brief 定跡ウィンドウクラスの定義
/// @todo remove コメントスタイルガイド適用済み


#include <QWidget>
#include <QPushButton>
#include <QToolButton>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QTableWidget>
#include <QMap>
#include <QVector>
#include <QCloseEvent>
#include <QShowEvent>
#include <QMenu>
#include <QStringList>
#include <QAction>
#include <QFrame>
#include <QSet>

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
    
    /**
     * @brief 棋譜データを要求するシグナル
     * 
     * MainWindowがこのシグナルを受け取り、setKifuDataForMerge()で応答する
     */
    void requestKifuDataForMerge();

public slots:
    /**
     * @brief 「開く」ボタンがクリックされたときのスロット
     */
    void onOpenButtonClicked();
    
    /**
     * @brief 「新規作成」ボタンがクリックされたときのスロット
     */
    void onNewButtonClicked();
    
    /**
     * @brief 「上書保存」ボタンがクリックされたときのスロット
     */
    void onSaveButtonClicked();
    
    /**
     * @brief 「名前を付けて保存」ボタンがクリックされたときのスロット
     */
    void onSaveAsButtonClicked();
    
    /**
     * @brief 「定跡手追加」ボタンがクリックされたときのスロット
     */
    void onAddMoveButtonClicked();
    
    /**
     * @brief 棋譜データを受け取るスロット（MainWindowから呼ばれる）
     * @param sfenList 各手番のSFEN文字列リスト
     * @param moveList 各手のUSI形式指し手リスト
     * @param japaneseMoveList 各手の日本語表記リスト
     * @param currentPly 現在選択中の手数
     */
    void setKifuDataForMerge(const QStringList &sfenList, 
                             const QStringList &moveList,
                             const QStringList &japaneseMoveList,
                             int currentPly);
    
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
     * @brief 最近使ったファイル履歴をクリアするスロット
     */
    void onClearRecentFilesClicked();

    /**
     * @brief 「着手」ボタンがクリックされたときのスロット
     */
    void onPlayButtonClicked();
    
    /**
     * @brief 「編集」ボタンがクリックされたときのスロット
     */
    void onEditButtonClicked();
    
    /**
     * @brief 「削除」ボタンがクリックされたときのスロット
     */
    void onDeleteButtonClicked();
    
    /**
     * @brief 自動読込チェックボックスの状態が変更されたときのスロット
     */
    void onAutoLoadCheckBoxChanged(Qt::CheckState state);
    
    /**
     * @brief 「停止」ボタンがクリックされたときのスロット
     */
    void onStopButtonClicked();
    
    
    /**
     * @brief 最近使ったファイルメニューの項目がクリックされたときのスロット
     */
    void onRecentFileClicked();
    
    /**
     * @brief 現在の棋譜から定跡をマージする
     */
    void onMergeFromCurrentKifu();
    
    /**
     * @brief 棋譜ファイルから定跡をマージする
     */
    void onMergeFromKifuFile();
    
    /**
     * @brief テーブルがダブルクリックされた時のスロット
     */
    void onTableDoubleClicked(int row, int column);
    
    /**
     * @brief テーブルのコンテキストメニュー表示
     */
    void onTableContextMenu(const QPoint &pos);
    
    /**
     * @brief コンテキストメニュー: 着手
     */
    void onContextMenuPlay();
    
    /**
     * @brief コンテキストメニュー: 編集
     */
    void onContextMenuEdit();
    
    /**
     * @brief コンテキストメニュー: 削除
     */
    void onContextMenuDelete();
    
    /**
     * @brief コンテキストメニュー: 指し手をコピー
     */
    void onContextMenuCopyMove();

protected:
    /**
     * @brief ウィンドウが閉じられる時に設定を保存
     */
    void closeEvent(QCloseEvent *event) override;

    /**
     * @brief ウィンドウが表示される時に遅延読込を実行
     */
    void showEvent(QShowEvent *event) override;

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
    
    /**
     * @brief ステータス表示を更新
     */
    void updateStatusDisplay();
    
    /**
     * @brief 局面サマリー表示を更新
     */
    void updatePositionSummary();
    
    /**
     * @brief 定跡データをファイルに保存する
     * @param filePath 保存先ファイルパス
     * @return 保存成功時true
     */
    bool saveJosekiFile(const QString &filePath);
    
    /**
     * @brief 編集状態が変更された時にウィンドウタイトルを更新
     */
    void updateWindowTitle();
    
    /**
     * @brief 編集状態をセット
     * @param modified true=変更あり, false=変更なし
     */
    void setModified(bool modified);
    
    /**
     * @brief 未保存の変更がある場合に保存確認ダイアログを表示
     * @return true=続行可能, false=キャンセル
     */
    bool confirmDiscardChanges();
    
    /**
     * @brief 最近使ったファイルリストを更新
     * @param filePath 新たに開いた/保存したファイルパス
     */
    void addToRecentFiles(const QString &filePath);
    
    /**
     * @brief 最近使ったファイルメニューを更新
     */
    void updateRecentFilesMenu();
    
    /**
     * @brief マージダイアログからの登録要求を処理
     * @param sfen 正規化されたSFEN
     * @param sfenWithPly 手数付きSFEN
     * @param usiMove USI形式の指し手
     */
    void onMergeRegisterMove(const QString &sfen, const QString &sfenWithPly, const QString &usiMove);

    // === ファイルグループ ===
    QPushButton  *m_openButton;        ///< 「開く」ボタン
    QPushButton  *m_newButton;         ///< 「新規作成」ボタン
    QPushButton  *m_saveButton;        ///< 「上書保存」ボタン
    QPushButton  *m_saveAsButton;      ///< 「名前を付けて保存」ボタン
    QPushButton  *m_recentButton;      ///< 「最近使ったファイル」ボタン
    QMenu        *m_recentFilesMenu;   ///< 最近使ったファイルメニュー
    QLabel       *m_filePathLabel;     ///< 選択されたファイルパスを表示するラベル
    QLabel       *m_fileStatusLabel;   ///< ファイル読込状態ラベル（✓読込済 / ✗未読込）
    
    // === 表示設定グループ ===
    QPushButton  *m_fontIncreaseBtn;   ///< フォント拡大ボタン
    QPushButton  *m_fontDecreaseBtn;   ///< フォント縮小ボタン
    QCheckBox    *m_autoLoadCheckBox;  ///< 自動読込チェックボックス
    
    // === 操作グループ ===
    QPushButton  *m_stopButton;        ///< 定跡表示停止ボタン
    QPushButton  *m_addMoveButton;     ///< 定跡手追加ボタン
    QToolButton  *m_mergeButton;       ///< マージボタン（ドロップダウンメニュー付き）
    QMenu        *m_mergeMenu;         ///< マージメニュー
    
    // === 状態表示 ===
    QLabel       *m_currentSfenLabel;  ///< 現在の局面のSFEN表示用ラベル
    QLabel       *m_sfenLineLabel;     ///< 定跡ファイルのSFEN行表示用ラベル
    QLabel       *m_statusLabel;       ///< ステータスバーラベル
    QLabel       *m_positionSummaryLabel; ///< 局面サマリー表示ラベル
    QLabel       *m_emptyGuideLabel;   ///< 空状態ガイダンスラベル
    QLabel       *m_noticeLabel;       ///< 注意書きラベル
    QPushButton  *m_showSfenDetailBtn; ///< SFEN詳細表示ボタン
    QWidget      *m_sfenDetailWidget;  ///< SFEN詳細表示ウィジェット
    
    // === コンテキストメニュー ===
    QMenu        *m_tableContextMenu;  ///< テーブルコンテキストメニュー
    QAction      *m_actionPlay;        ///< 着手アクション
    QAction      *m_actionEdit;        ///< 編集アクション
    QAction      *m_actionDelete;      ///< 削除アクション
    QAction      *m_actionCopyMove;    ///< 指し手コピーアクション
    
    // === テーブル ===
    QTableWidget *m_tableWidget;       ///< 定跡表示用テーブル
    
    // === データ ===
    QString       m_currentFilePath;   ///< 現在選択されているファイルパス
    QString       m_currentSfen;       ///< 現在の局面のSFEN
    int           m_fontSize;          ///< フォントサイズ
    bool          m_humanCanPlay;      ///< 人間が着手可能かどうか
    bool          m_autoLoadEnabled;   ///< 定跡ファイル自動読込が有効かどうか
    bool          m_displayEnabled;    ///< 定跡表示が有効かどうか
    bool          m_modified;          ///< 編集状態（変更があるか）
    
    /// 最近使ったファイルリスト（最大5件）
    QStringList   m_recentFiles;
    
    /// 現在表示中の定跡手リスト（着手ボタンから参照）
    QVector<JosekiMove> m_currentMoves;

    /// 定跡データ（正規化SFEN → 指し手リスト）
    QMap<QString, QVector<JosekiMove>> m_josekiData;
    
    /// 元のSFEN（手数付き）を保持（正規化SFEN → 元のSFEN）
    QMap<QString, QString> m_sfenWithPlyMap;
    
    /// マージダイアログで登録済みの指し手セット（「正規化SFEN:USI指し手」形式）
    QSet<QString> m_mergeRegisteredMoves;

    /// 遅延読込用：初回表示時に自動読込を行うかどうか
    bool          m_pendingAutoLoad = false;

    /// 遅延読込用：自動読込するファイルパス
    QString       m_pendingAutoLoadPath;
};

#endif // JOSEKIWINDOW_H
