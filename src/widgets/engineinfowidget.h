#ifndef ENGINEINFOWIDGET_H
#define ENGINEINFOWIDGET_H

/// @file engineinfowidget.h
/// @brief エンジン情報表示ウィジェットクラスの定義


#include <QWidget>
#include <QList>
class QTableWidget;
class QToolButton;
class QHBoxLayout;
class UsiCommLogModel;

class EngineInfoWidget : public QWidget {
    Q_OBJECT
public:
    explicit EngineInfoWidget(QWidget* parent=nullptr, bool showFontButtons=false, bool showPredictedMove=true);
    void setModel(UsiCommLogModel* model);
    void setDisplayNameFallback(const QString& name);
    
    // フォントサイズ設定
    void setFontSize(int pointSize);
    int fontSize() const { return m_fontSize; }
    
    // ウィジェットインデックス（設定保存用）
    void setWidgetIndex(int index) { m_widgetIndex = index; }
    int widgetIndex() const { return m_widgetIndex; }
    
    // 列幅の取得・設定
    QList<int> columnWidths() const;
    void setColumnWidths(const QList<int>& widths);
    
    // 列数の取得
    int columnCount() const { return COL_COUNT; }

signals:
    // フォントサイズ変更シグナル
    void fontSizeIncreaseRequested();
    void fontSizeDecreaseRequested();
    
    // 列幅変更シグナル
    void columnWidthChanged();

protected:
    // リサイズイベント（エンジン名列の自動調整用）
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
    
    // 列幅変更時のスロット
    void onSectionResized(int logicalIndex, int oldSize, int newSize);

private:
    UsiCommLogModel* m_model=nullptr;
    QTableWidget* m_table=nullptr;
    QString m_fallbackName;
    int m_fontSize=10;
    int m_widgetIndex=0;  // ウィジェットインデックス
    bool m_columnWidthsLoaded=false;  // 列幅が設定ファイルから読み込まれたか
    int m_buttonRowHeight=0;  // ボタン行の高さ（ボタン非表示時は0）

    // フォントサイズボタン
    QToolButton* m_btnFontDecrease=nullptr;
    QToolButton* m_btnFontIncrease=nullptr;
    bool m_showFontButtons=false;
    bool m_showPredictedMove=true;  // 予想手列を表示するか
    
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

    // コンストラクタ分割メソッド
    void setupTable();
    void initializeCells();
    void buildLayout();

    // エンジン名列の幅を残りスペースに合わせて調整
    void adjustEngineNameColumn();

    // ヘッダースタイルの再適用（フォントサイズ反映用）
    void applyHeaderStyle();
};

#endif // ENGINEINFOWIDGET_H
