#ifndef GAMEINFOEDITMODEL_H
#define GAMEINFOEDITMODEL_H

/// @file gameinfoeditmodel.h
/// @brief 対局情報の表の中身と編集の履歴

#include <QAbstractTableModel>
#include <QList>
#include <QString>

#include "kifparsetypes.h"  // KifGameInfoItem

/**
 * @brief 対局情報（項目名と内容の2列）を編集する表のモデル
 *
 * - 読み込んだ項目の名前は編集できない。行を追加した項目は名前も編集できる。
 * - 利用者の編集・行の追加と削除は1操作ずつ履歴に積み、undo()/redo() で行き来する。
 * - 未反映の変更は、反映済みの内容（setItems() か markApplied() の時点）と比べて決める。
 * - 自動で書き換える値（対局者名・終了日時など）は、反映済みの内容と履歴にも書き込むので、
 *   未反映の変更に数えず、元に戻しても古い値に戻らない。
 *
 * 表示用の値（Qt::DisplayRole・Qt::EditRole）は書いたままの文字列。項目名・値の表記の切り替えは
 * Qt::AccessibleTextRole・Qt::ToolTipRole と、表の描画（GameInfoPane）で行う。
 */
class GameInfoEditModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column { KeyColumn, ValueColumn, ColumnCount };
    /// 内容が空のときに薄く出す案内文（表示だけで、値・コピー・棋譜には含めない）
    static constexpr int PlaceholderRole = Qt::UserRole + 1;

    explicit GameInfoEditModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    /// 読み込んだ対局情報で置き換え、反映済みにして履歴を消す
    /// （beforeMatchStart: 対局前の案内文「対局開始時に設定」などを出す）
    void setItems(const QList<KifGameInfoItem>& items, bool beforeMatchStart = false);
    QList<KifGameInfoItem> items() const;

    bool isDirty() const;
    /// 今の内容を反映済みにする
    void markApplied();

    /// 自動で決まる値を書く。項目が無ければ末尾に足す（その項目名は編集できない）
    void setAutomaticValue(const QString& key, const QString& value);

    /// 末尾に項目名も編集できる空の行を足し、その行番号を返す
    int appendRow();
    void removeRowAt(int row);

    bool canUndo() const { return m_historyIndex > 0; }
    bool canRedo() const { return m_historyIndex + 1 < m_history.size(); }
    void undo();
    void redo();

signals:
    /// 履歴か未反映の変更の有無が変わったかもしれない（操作ボタンの状態を合わせる）
    void editStateChanged();

private:
    struct Row {
        QString key;
        QString value;
        bool keyEditable = false;
        bool operator==(const Row& other) const
        {
            return key == other.key && value == other.value && keyEditable == other.keyEditable;
        }
    };
    using Rows = QList<Row>;

    void resetRows(const Rows& rows);
    void recordEdit();
    void resetHistory();
    QString placeholderFor(const QString& key) const;
    QString presentedText(int row, int column) const;

    Rows m_rows;
    QList<KifGameInfoItem> m_applied;   ///< 反映済みの内容
    QList<Rows> m_history;              ///< 編集の履歴（m_history[m_historyIndex] が今の内容）
    int m_historyIndex = 0;
    bool m_beforeMatchStart = false;
};

#endif // GAMEINFOEDITMODEL_H
