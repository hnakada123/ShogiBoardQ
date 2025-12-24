#ifndef ENGINEINFOWIDGET_H
#define ENGINEINFOWIDGET_H

#include <QWidget>
#include <QList>
class QTableWidget;
class QToolButton;
class QHBoxLayout;
class UsiCommLogModel;

class EngineInfoWidget : public QWidget {
    Q_OBJECT
public:
    explicit EngineInfoWidget(QWidget* parent=nullptr, bool showFontButtons=false);
    void setModel(UsiCommLogModel* model);
    void setDisplayNameFallback(const QString& name);
    
    // ★ 追加: フォントサイズ設定
    void setFontSize(int pointSize);
    int fontSize() const { return m_fontSize; }
    
    // ★ 追加: ウィジェットインデックス（設定保存用）
    void setWidgetIndex(int index) { m_widgetIndex = index; }
    int widgetIndex() const { return m_widgetIndex; }
    
    // ★ 追加: 列幅の取得・設定
    QList<int> columnWidths() const;
    void setColumnWidths(const QList<int>& widths);
    
    // ★ 追加: 列数の取得
    int columnCount() const { return COL_COUNT; }

signals:
    // ★ 追加: フォントサイズ変更シグナル
    void fontSizeIncreaseRequested();
    void fontSizeDecreaseRequested();
    
    // ★ 追加: 列幅変更シグナル
    void columnWidthChanged();

protected:
    // ★ 追加: リサイズイベント（エンジン名列の自動調整用）
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;

private slots:
    void onNameChanged();
    void onPredChanged();
    void onSearchedChanged();
    void onDepthChanged();
    void onNodesChanged();
    void onNpsChanged();
    void onHashChanged();
    
    // ★ 追加: 列幅変更時のスロット
    void onSectionResized(int logicalIndex, int oldSize, int newSize);

private:
    UsiCommLogModel* m_model=nullptr;
    QTableWidget* m_table=nullptr;
    QString m_fallbackName;
    int m_fontSize=10;
    int m_widgetIndex=0;  // ★ 追加: ウィジェットインデックス
    bool m_columnWidthsLoaded=false;  // ★ 追加: 列幅が設定ファイルから読み込まれたか
    
    // ★ 追加: フォントサイズボタン
    QToolButton* m_btnFontDecrease=nullptr;
    QToolButton* m_btnFontIncrease=nullptr;
    bool m_showFontButtons=false;
    
    // 列インデックス
    enum Column {
        COL_ENGINE_NAME = 0,
        COL_PRED,
        COL_SEARCHED,
        COL_DEPTH,
        COL_NODES,
        COL_NPS,
        COL_HASH,
        COL_COUNT
    };
    
    void setCellValue(int col, const QString& value);
    
    // ★ 追加: エンジン名列の幅を残りスペースに合わせて調整
    void adjustEngineNameColumn();
};

#endif // ENGINEINFOWIDGET_H
