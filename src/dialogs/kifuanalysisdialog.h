#ifndef KIFUANALYSISDIALOG_H
#define KIFUANALYSISDIALOG_H

/// @file kifuanalysisdialog.h
/// @brief 棋譜解析ダイアログクラスの定義

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
    using Engine = EngineListSettings::EngineEntry;

    /// 選んでいるエンジン（未登録なら std::nullopt）
    std::optional<Engine> engine() const;

    // "開始局面から最終手まで"を選択したかどうかのフラグを取得する。
    bool initPosition() const;
    
    // 範囲指定の開始手数を取得する（0から）
    int startPly() const;
    
    // 範囲指定の終了手数を取得する
    int endPly() const;
    
    // 最大手数を設定する（ダイアログ表示前に呼び出す）
    void setMaxPly(int maxPly);

    // 1手あたりの思考時間（秒数）を取得する。
    int byoyomiSec() const;

protected:
    void done(int result) override;

private slots:
    /// 選んだエンジンの設定ダイアログを開く
    void showEngineSettingsDialog(const EngineListSettings::EngineEntry& engine);

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

    // "開始局面から最終手まで"を選択した場合、true
    bool m_initPosition = true;

    // 範囲指定の開始手数
    int m_startPly = 0;

    // 範囲指定の終了手数
    int m_endPly = 0;

    // 最大手数
    int m_maxPly = 0;

    // 設定から読み込んだ開始・終了手数（setMaxPly時に適用）
    int m_savedStartPly = 0;
    int m_savedEndPly = 0;

    // 1手あたりの思考時間（秒数）
    int m_byoyomiSec = 0;


    // 範囲指定の数値欄の前後に置く文字列を設定する
    void applyRangeLabels();
};

#endif // KIFUANALYSISDIALOG_H
