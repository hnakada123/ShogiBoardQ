#ifndef ENGINEINFOWIDGET_H
#define ENGINEINFOWIDGET_H

/// @file engineinfowidget.h
/// @brief エンジン情報表示ウィジェットクラスの定義


#include <QWidget>
#include <QFont>
#include <QList>
#include <QStringList>

#include "analysissettings.h"

class ColumnWidthMemory;
class QTableWidget;
class UsiCommLogModel;

/**
 * @brief エンジン名・予想手・探索手・深さ・ノード数などを1行で出す情報欄
 *
 * 想定桁数と指し手が収まる幅を確保し、内容が長くなったときだけ広げる。書体が変わるまでは縮めない。
 * 利用者が見出しの境目をドラッグして列幅を変えると、その列の幅を思考の表の組（table）ごとに保存し
 * （ColumnWidthMemory）、その列だけは以後も自動で変えない。
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
    
    /// 今の列幅
    QList<int> columnWidths() const;
    
    // 列数の取得
    int columnCount() const { return COL_COUNT; }

protected:
    // 情報帯を表示幅に合わせる
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;

private slots:
    void onNameChanged();
    /// 探索状況・予想手を出す（数値の書式はここで決める）
    void onStatusChanged();

private:
    UsiCommLogModel* m_model=nullptr;
    QTableWidget* m_table=nullptr;
    QString m_fallbackName;
    int m_fontSize=10;
    AnalysisSettings::ThinkingTable m_thinkingTable;  // 列幅を保存する組
    ColumnWidthMemory* m_columnWidths=nullptr;  // 利用者が決めた列幅（0 の列は内容に合わせる。見出しが親）
    bool m_showPredictedMove=true;  // 予想手列を表示するか
    bool m_tableGeometryPending=false;  // 列幅・高さの計算を予約済みか

    // 列幅の計算で測った文字の幅。思考情報は1秒に何度も届くので、文字と書体が同じなら測り直さない
    struct MeasuredText {
        QString text;
        int width = -1;  // -1 は未計測
    };
    QList<MeasuredText> m_cellWidths;     // 列ごとの値の幅
    QList<MeasuredText> m_headingWidths;  // 列ごとの見出しの幅
    QList<int> m_autoColumnWidths;       // 初期幅と受信済みの内容が収まる幅（書体変更時だけ再計算）
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
