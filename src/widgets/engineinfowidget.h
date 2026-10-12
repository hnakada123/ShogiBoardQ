#ifndef ENGINEINFOWIDGET_H
#define ENGINEINFOWIDGET_H

/// @file engineinfowidget.h
/// @brief エンジン情報表示ウィジェットクラスの定義


#include <QWidget>
#include <QFont>
#include <QList>
#include <QStringList>
class QTableWidget;
class UsiCommLogModel;

class EngineInfoWidget : public QWidget {
    Q_OBJECT
public:
    explicit EngineInfoWidget(QWidget* parent=nullptr, bool showPredictedMove=true);
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
    // 列幅変更シグナル
    void columnWidthChanged();

protected:
    // 情報帯を表示幅に合わせる
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
    bool m_showPredictedMove=true;  // 予想手列を表示するか
    bool m_tableGeometryPending=false;  // 列幅・高さの計算を予約済みか

    // 列幅の計算で測った文字の幅。思考情報は1秒に何度も届くので、文字と書体が同じなら測り直さない
    struct MeasuredText {
        QString text;
        int width = -1;  // -1 は未計測
    };
    QList<MeasuredText> m_cellWidths;     // 列ごとの値の幅
    QList<MeasuredText> m_headingWidths;  // 列ごとの見出しの幅
    QFont m_measuredHeadingFont;          // 幅を測った見出し・値の書体
    QFont m_measuredMoveFont;             // 幅を測った指し手の書体

    // 見出しの高さを測った条件（書体・スタイルシート・見出し・表示する列）
    struct HeaderHeightKey {
        QFont font;
        QString styleSheet;
        QStringList headings;
        quint32 hiddenColumns = 0;  // 隠した列のビット
        bool sameAs(const HeaderHeightKey& other) const;
    };
    HeaderHeightKey m_headerHeightKey;
    bool m_headerHeightMeasured = false;
    int m_headerHeight = 0;
    
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

    // 内容に合う列幅と情報帯の高さを計算（手動の列幅は維持）
    void updateTableGeometry();
    // 思考情報の1行で複数の値が変わるため、計算はまとめて1回にする
    void scheduleTableGeometry();
    // 見出しの高さ（条件が同じなら前回の値を使う）
    int headerHeight();

    // ヘッダースタイルの再適用（フォントサイズ反映用）
    void applyHeaderStyle();
};

#endif // ENGINEINFOWIDGET_H
