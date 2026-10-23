#ifndef KIFUANALYSISDIALOG_H
#define KIFUANALYSISDIALOG_H

/// @file kifuanalysisdialog.h
/// @brief 棋譜解析ダイアログクラスの定義

#include "analysisjob.h"
#include "enginelistsettings.h"
#include <QDialog>
#include <memory>
#include <optional>

class EngineSelector;

namespace Ui {
class KifuAnalysisDialog;
}

// 棋譜解析ダイアログのUIを設定する。
class KifuAnalysisDialog : public QDialog
{
    Q_OBJECT

public:
    // コンストラクタ
    explicit KifuAnalysisDialog(QWidget *parent = nullptr);
    ~KifuAnalysisDialog() override;

    /// 登録済みエンジン（名前・実行ファイル・作者）
    using Engine = EngineEntry;

    /// 選んでいるエンジン（未登録なら std::nullopt）
    std::optional<Engine> engine() const;

    /// 選んだ解析の範囲と1局面の思考時間（「開始局面から最終手まで」なら 0〜最大手数。範囲は最大手数に収める）
    AnalysisRequest::Options options() const { return m_options; }

    // 最大手数を設定する（ダイアログ表示前に呼び出す）
    void setMaxPly(int maxPly);

private slots:
    /// 選んだエンジンの設定ダイアログを開く
    void showEngineSettingsDialog(const EngineEntry& engine);

    // OKボタンが押された場合、エンジン名、エンジン番号、解析局面フラグ、思考時間を取得する。
    void processEngineSettings();
    
    // 範囲指定ラジオボタンが選択された場合
    void onRangeRadioToggled(bool checked);
    
    // 開始手数が変更された場合
    void onStartPlyChanged(int value);

    void updateSummary();

private:
    // UI
    std::unique_ptr<Ui::KifuAnalysisDialog> ui;

    /// エンジンの選択（コンボ・設定ボタン・未登録の案内。QObject parent 所有）
    EngineSelector* m_engineSelector = nullptr;

    /// OK で決めた範囲と思考時間
    AnalysisRequest::Options m_options;

    // 最大手数
    int m_maxPly = 0;

    // 設定から読み込んだ開始・終了手数（setMaxPly時に適用）
    int m_savedStartPly = 0;
    int m_savedEndPly = 0;


    // 範囲指定の数値欄の前後に置く文字列を設定する
    void applyRangeLabels();
};

#endif // KIFUANALYSISDIALOG_H
