#ifndef THINKINGPANEL_H
#define THINKINGPANEL_H

/// @file thinkingpanel.h
/// @brief 思考ドックの中身（エンジン2つ分の情報と思考の表、候補手の矢印の表示設定）

#include <QWidget>

#include "fontsizehelper.h"
#include "shogienginethinkingmodel.h"

class EngineInfoWidget;
class QCheckBox;
class QLabel;
class ThinkingTableView;
class UsiCommLogModel;

/**
 * @brief 対局・棋譜解析のエンジンの思考を出す思考ドックの中身
 *
 * エンジンごとに名前などの情報欄と思考の表を持つ（2つ目のエンジンは使うときだけ出す）。
 * 上部に文字の大きさのボタンと、対局中の候補手の矢印の表示設定を置く。
 * 表の体裁と列幅は ThinkingTableView、情報欄の列幅は EngineInfoWidget が受け持つ。
 */
class ThinkingPanel : public QWidget
{
    Q_OBJECT

public:
    explicit ThinkingPanel(QWidget* parent = nullptr);

    /// 表に出す思考情報と、情報欄に出すエンジン名・状態（log）
    void setModels(ShogiEngineThinkingModel* thinking1, ShogiEngineThinkingModel* thinking2,
                   UsiCommLogModel* log1, UsiCommLogModel* log2);

    /// 2つ目のエンジンの情報欄と表を出すか
    void setSecondEngineVisible(bool on);

    EngineInfoWidget* info1() const { return m_info1; }
    EngineInfoWidget* info2() const { return m_info2; }
    ThinkingTableView* view1() const { return m_view1; }
    ThinkingTableView* view2() const { return m_view2; }

public slots:
    /// 候補手の矢印の表示状態（検討中は対局の矢印の設定を使わない。先読みの説明を添える）
    void setArrowDisplayState(bool considerationActive, const QString& ponderDescription);
    /// model を出している表の選択を外す（読み筋ダイアログを閉じたとき）
    void clearSelection(const ShogiEngineThinkingModel* model);

signals:
    /// 対局の矢印・先読みの矢印の表示設定が変わった（矢印を描き直す）
    void arrowSettingsChanged();
    /// 読み筋の行の「盤面」ボタンが押された
    void pvRowClicked(ShogiEngineThinkingModel* model, int row);

private slots:
    void onMatchArrowsToggled(bool checked);
    void onPonderArrowsToggled(bool checked);
    void applyFontSize();

private:
    void updateArrowControls();

    EngineInfoWidget* m_info1 = nullptr;
    EngineInfoWidget* m_info2 = nullptr;
    ThinkingTableView* m_view1 = nullptr;
    ThinkingTableView* m_view2 = nullptr;
    FontSizeHelper m_fontSize;   ///< 表と情報欄の文字の大きさ（変更のたびに保存する）
    QCheckBox* m_matchArrows = nullptr;
    QCheckBox* m_ponderArrows = nullptr;
    QLabel* m_arrowDescription = nullptr;
    bool m_arrowConsideration = false;   ///< 検討中か（検討の矢印は検討タブで設定する）
    QString m_ponderDescription;         ///< 先読みの矢印の説明
};

#endif // THINKINGPANEL_H
