#ifndef ENGINEINFOWIDGET_H
#define ENGINEINFOWIDGET_H

#include <QWidget>
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

signals:
    // ★ 追加: フォントサイズ変更シグナル
    void fontSizeIncreaseRequested();
    void fontSizeDecreaseRequested();

private slots:
    void onNameChanged();
    void onPredChanged();
    void onSearchedChanged();
    void onDepthChanged();
    void onNodesChanged();
    void onNpsChanged();
    void onHashChanged();

private:
    UsiCommLogModel* m_model=nullptr;
    QTableWidget* m_table=nullptr;
    QString m_fallbackName;
    int m_fontSize=10;
    
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
};

#endif // ENGINEINFOWIDGET_H
