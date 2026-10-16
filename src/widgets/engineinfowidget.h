#ifndef ENGINEINFOWIDGET_H
#define ENGINEINFOWIDGET_H

/// @file engineinfowidget.h
/// @brief エンジン情報表示ウィジェットクラスの定義


#include <QWidget>
#include <QFont>
#include <QList>
#include <QStringList>

#include "analysissettings.h"

class QTableWidget;
class UsiCommLogModel;

/**
 * @brief エンジン名・予想手・探索手・深さ・ノード数などを1行で出す情報欄
 *
 * 列幅は内容に合わせて広げる。利用者が見出しの境目をドラッグして列幅を変えると、
 * その幅を思考の表の組（table）ごとに保存し、以後は自動で変えない。
 */
class EngineInfoWidget : public QWidget {
    Q_OBJECT
public:
    explicit EngineInfoWidget(AnalysisSettings::ThinkingTable table, QWidget* parent = nullptr,
                              bool showPredictedMove = true);
    void setModel(UsiCommLogModel* model);
    void setDisplayNameFallback(const QString& name);
    
    // フォントサイズ設定
    void setFontSize(int pointSize);
    int fontSize() const { return m_fontSize; }
    
    // 列幅の取得・設定（設定した幅は内容に合わせて変えない）
    QList<int> columnWidths() const;
    void setColumnWidths(const QList<int>& widths);
    
    // 列数の取得
    int columnCount() const { return COL_COUNT; }

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
    
    // 利用者が列幅を変えたら保存する
    void onSectionResizedByUser();

private:
    UsiCommLogModel* m_model=nullptr;
    QTableWidget* m_table=nullptr;
    QString m_fallbackName;
    int m_fontSize=10;
    AnalysisSettings::ThinkingTable m_thinkingTable;  // 列幅を保存する組
    bool m_fixedColumnWidths=false;  // 保存した幅・利用者が決めた幅を使うか（内容に合わせない）
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
