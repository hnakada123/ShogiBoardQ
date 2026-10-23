#ifndef PLAYERSETUPWIDGET_H
#define PLAYERSETUPWIDGET_H

/// @file playersetupwidget.h
/// @brief 対局開始ダイアログの対局者1人の欄（人間かエンジンか・名前・エンジンの設定）

#include <QWidget>

#include "gamesettings.h"
#include "startgamedatabridge.h"

class EngineSelector;
class QCheckBox;
class QLineEdit;
class QPushButton;
class QStackedWidget;

/**
 * @brief 対局者1人を選ぶ欄
 *
 * コンボの先頭が「人間」、その後に登録エンジンが並ぶ（EngineSelector の内蔵の項目を「人間」に使う）。
 * 人間なら名前の入力欄、エンジンなら「エンジン設定」ボタンと「定跡を使う」・「条件…」を出す。
 * 設定ダイアログ・定跡の条件のダイアログは受け手が開く。定跡は定跡ウィンドウで開いている定跡を使い、
 * 開いていなければ「定跡を使う」を押せない（setJosekiBookName）。
 */
class PlayerSetupWidget : public QWidget
{
    Q_OBJECT

public:
    /// side は部品の objectName に付ける番号（1=先手／下手、2=後手／上手。例: comboBoxPlayer1）。
    /// saved はこの対局者の保存先（エンジンの選択の保存・復元に使う）
    PlayerSetupWidget(int side, const GameSettings::StartGame::Player& saved, QWidget* parent = nullptr);

    /// 選んだ対局者（時間は含まない）。定跡は、エンジンで、定跡を開いていて「定跡を使う」のときだけ使う
    PlayerChoice choice() const;
    /// 対局者を選ぶ（人間・名前・エンジンの名前。時間と定跡の条件は見ない）
    void setChoice(const PlayerChoice& choice);
    /// 定跡を使う条件（enabled は「定跡を使う」のチェック。定跡を開いていなくてもチェックのまま返す）
    JosekiPlayOptions josekiOptions() const;
    void setJosekiOptions(const JosekiPlayOptions& options);
    /// 定跡ウィンドウで開いている定跡の名前（空なら開いていないので「定跡を使う」を押せない）
    void setJosekiBookName(const QString& name);
    bool isEngine() const;

    /// 保存した選択を読む（保存したエンジンが一覧に無ければ人間にする）
    void load();
    /// 選択を保存する（人間を選んでいるときは、前に選んだエンジンを残す）
    void save() const;
    /// 既定値に戻す（engine なら先頭の登録エンジン。無ければ人間）
    void reset(bool engine);

signals:
    /// 人間かエンジンかが変わった
    void kindChanged();
    /// 「エンジン設定」が押された
    void engineSettingsRequested(const EngineEntry& engine);
    /// 定跡の「条件…」が押された（side はコンストラクタに渡した番号）
    void josekiConditionsRequested(int side);

private slots:
    void updatePages();
    void onJosekiConditionsClicked();

private:
    const GameSettings::StartGame::Player& m_saved;
    int m_side = 1;
    JosekiPlayOptions m_joseki;   ///< 定跡を使う条件（enabled はチェックが持つ）
    EngineSelector* m_selector = nullptr;
    QLineEdit* m_humanName = nullptr;
    QStackedWidget* m_labelPages = nullptr;
    QStackedWidget* m_inputPages = nullptr;
    QPushButton* m_engineSettings = nullptr;
    QCheckBox* m_useJoseki = nullptr;
    QPushButton* m_josekiConditions = nullptr;
};

#endif // PLAYERSETUPWIDGET_H
