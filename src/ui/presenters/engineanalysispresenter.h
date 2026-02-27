#ifndef ENGINEANALYSISPRESENTER_H
#define ENGINEANALYSISPRESENTER_H

/// @file engineanalysispresenter.h
/// @brief エンジン解析思考ビュープレゼンタの定義

#include <QObject>
#include <memory>

class QTableView;
class QAbstractItemModel;
class QModelIndex;

class EngineInfoWidget;
class LogViewFontManager;
class ShogiEngineThinkingModel;

/**
 * @brief 思考タブのビュー管理・データ表示を担うプレゼンタ
 *
 * EngineAnalysisTab から思考タブ（view1/view2, info1/info2）の
 * モデル接続、列幅管理、フォントサイズ管理、数値フォーマットを分離したクラス。
 */
class EngineAnalysisPresenter : public QObject
{
    Q_OBJECT

public:
    explicit EngineAnalysisPresenter(QObject* parent = nullptr);
    ~EngineAnalysisPresenter() override;

    /// ビュー・情報ウィジェットの参照を設定
    void setViews(QTableView* v1, QTableView* v2,
                  EngineInfoWidget* i1, EngineInfoWidget* i2);

    /// 思考モデルを設定（列幅・数値フォーマット適用含む）
    void setModels(ShogiEngineThinkingModel* m1, ShogiEngineThinkingModel* m2);

    /// エンジン1の思考モデルを個別設定
    void setEngine1ThinkingModel(ShogiEngineThinkingModel* m);

    /// エンジン2の思考モデルを個別設定
    void setEngine2ThinkingModel(ShogiEngineThinkingModel* m);

    /// 思考ビューの選択をクリア（engineIndex: 0=E1, 1=E2, 2=検討）
    void clearThinkingViewSelection(int engineIndex, QTableView* considerationView);

    /// ヘッダの基本設定（モデル設定前でもOK）
    void setupThinkingViewHeader(QTableView* v);

    /// フォントマネージャーの初期化
    void initThinkingFontManager();

    /// EngineInfoWidget の列幅を設定から読み込み適用
    void loadEngineInfoColumnWidths();

    /// EngineInfoWidget の列幅変更シグナルを接続
    void wireEngineInfoColumnWidthSignals();

    /// 思考ビューのクリック・列リサイズシグナルを接続
    void wireThinkingViewSignals();

    ShogiEngineThinkingModel* model1() const { return m_model1; }
    ShogiEngineThinkingModel* model2() const { return m_model2; }

signals:
    /// 読み筋がクリックされた時のシグナル（engineIndex: 0=E1, 1=E2）
    void pvRowClicked(int engineIndex, int row);

public slots:
    void onThinkingFontIncrease();
    void onThinkingFontDecrease();

private slots:
    void onModel1Reset();
    void onModel2Reset();
    void onView1Clicked(const QModelIndex& index);
    void onView2Clicked(const QModelIndex& index);
    void onEngineInfoColumnWidthChanged();
    void onView1SectionResized(int logicalIndex, int oldSize, int newSize);
    void onView2SectionResized(int logicalIndex, int oldSize, int newSize);

private:
    void reapplyViewTuning(QTableView* v, QAbstractItemModel* m);
    void applyThinkingViewColumnWidths(QTableView* v, int viewIndex);
    void onThinkingViewColumnWidthChanged(int viewIndex);

    static int findColumnByHeader(QAbstractItemModel* model, const QString& title);
    static void applyNumericFormattingTo(QTableView* view, QAbstractItemModel* model);

    QTableView* m_view1 = nullptr;
    QTableView* m_view2 = nullptr;
    EngineInfoWidget* m_info1 = nullptr;
    EngineInfoWidget* m_info2 = nullptr;

    ShogiEngineThinkingModel* m_model1 = nullptr;
    ShogiEngineThinkingModel* m_model2 = nullptr;

    int m_thinkingFontSize = 10;
    std::unique_ptr<LogViewFontManager> m_thinkingFontManager;

    bool m_thinkingView1WidthsLoaded = false;
    bool m_thinkingView2WidthsLoaded = false;
};

#endif // ENGINEANALYSISPRESENTER_H
