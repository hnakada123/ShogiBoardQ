#ifndef GAMEINFOPANECONTROLLER_H
#define GAMEINFOPANECONTROLLER_H

/// @file gameinfopanecontroller.h
/// @brief 対局情報ペインコントローラクラスの定義


#include <QObject>
#include <QList>
#include <QString>

#include "kifparsetypes.h"  // KifGameInfoItem

class QWidget;
class QTableWidget;
class QToolButton;
class QPushButton;
class QLabel;

/**
 * @brief 対局情報タブ（GameInfoPane）の管理を担当するコントローラクラス
 *
 * MainWindowから分離された責務:
 * - 対局情報テーブルの作成・管理
 * - フォントサイズ変更
 * - 編集状態の追跡（修正中インジケータ）
 * - Undo/Redo/Cut/Copy/Paste操作
 * - 対局情報の更新・保存
 */
class GameInfoPaneController : public QObject
{
    Q_OBJECT

public:
    explicit GameInfoPaneController(QObject* parent = nullptr);
    ~GameInfoPaneController() override;

    /// コンテナウィジェット（ツールバー＋テーブル）を取得
    QWidget* containerWidget() const;

    /// テーブルウィジェットを取得
    QTableWidget* tableWidget() const;

    /// 対局情報を設定（棋譜読み込み時など）
    void setGameInfo(const QList<KifGameInfoItem>& items);

    /// 現在の対局情報を取得
    QList<KifGameInfoItem> gameInfo() const;

    /// 元の対局情報（編集前）を取得
    QList<KifGameInfoItem> originalGameInfo() const;

    /// 対局者名を更新
    void updatePlayerNames(const QString& blackName, const QString& whiteName);

    /// 編集中かどうか
    bool isDirty() const;

    /// 未保存の編集がある場合に警告ダイアログを表示
    /// @return 続行してよい場合true、キャンセルの場合false
    bool confirmDiscardUnsaved();

    /// フォントサイズを取得
    int fontSize() const;

    /// フォントサイズを設定（設定ファイルにも保存）
    void setFontSize(int size);

public slots:
    /// フォントサイズを増加
    void increaseFontSize();

    /// フォントサイズを減少
    void decreaseFontSize();

    /// 元の内容に戻す（Undo）
    void undo();

    /// やり直す（Redo）- 編集中セルのredo
    void redo();

    /// 切り取り
    void cut();

    /// コピー
    void copy();

    /// 貼り付け
    void paste();

    /// 新しい行を追加
    void addRow();

    /// 対局情報を更新（編集を確定）
    void applyChanges();

signals:
    /// 対局情報が更新された（applyChanges後）
    void gameInfoUpdated(const QList<KifGameInfoItem>& items);

    /// 編集状態が変化した
    void dirtyStateChanged(bool dirty);

private slots:
    void onCellChanged(int row, int column);

private:
    void buildUi();
    void buildToolbar();
    void updateEditingIndicator();
    void applyFontSize();
    bool checkDirty() const;

    // UI部品
    QWidget*      m_container = nullptr;
    QWidget*      m_toolbar = nullptr;
    QTableWidget* m_table = nullptr;
    QToolButton*  m_btnFontIncrease = nullptr;
    QToolButton*  m_btnFontDecrease = nullptr;
    QToolButton*  m_btnUndo = nullptr;
    QToolButton*  m_btnRedo = nullptr;
    QToolButton*  m_btnCut = nullptr;
    QToolButton*  m_btnCopy = nullptr;
    QToolButton*  m_btnPaste = nullptr;
    QToolButton*  m_btnAddRow = nullptr;
    QLabel*       m_editingLabel = nullptr;
    QPushButton*  m_btnUpdate = nullptr;

    // 状態
    int  m_fontSize = 10;
    bool m_dirty = false;
    QList<KifGameInfoItem> m_originalItems;
};

#endif // GAMEINFOPANECONTROLLER_H
