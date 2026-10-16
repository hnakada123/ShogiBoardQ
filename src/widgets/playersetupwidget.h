#ifndef PLAYERSETUPWIDGET_H
#define PLAYERSETUPWIDGET_H

/// @file playersetupwidget.h
/// @brief 対局開始ダイアログの対局者1人の欄（人間かエンジンか・名前・エンジンの設定）

#include <QWidget>

#include "gamesettings.h"
#include "startgamedatabridge.h"

class EngineSelector;
class QLineEdit;
class QPushButton;
class QStackedWidget;

/**
 * @brief 対局者1人を選ぶ欄
 *
 * コンボの先頭が「人間」、その後に登録エンジンが並ぶ（EngineSelector の内蔵の項目を「人間」に使う）。
 * 人間なら名前の入力欄、エンジンなら「エンジン設定」ボタンを出す。設定ダイアログは受け手が開く。
 */
class PlayerSetupWidget : public QWidget
{
    Q_OBJECT

public:
    /// side は部品の objectName に付ける番号（1=先手／下手、2=後手／上手。例: comboBoxPlayer1）。
    /// saved はこの対局者の保存先（エンジンの選択の保存・復元に使う）
    PlayerSetupWidget(int side, const GameSettings::StartGame::Player& saved, QWidget* parent = nullptr);

    /// 選んだ対局者（時間は含まない）
    PlayerChoice choice() const;
    /// 対局者を選ぶ（人間・名前・エンジンの名前。時間は見ない）
    void setChoice(const PlayerChoice& choice);
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
    void engineSettingsRequested(const EngineListSettings::EngineEntry& engine);

private slots:
    void updatePages();

private:
    const GameSettings::StartGame::Player& m_saved;
    EngineSelector* m_selector = nullptr;
    QLineEdit* m_humanName = nullptr;
    QStackedWidget* m_labelPages = nullptr;
    QStackedWidget* m_inputPages = nullptr;
    QPushButton* m_engineSettings = nullptr;
};

#endif // PLAYERSETUPWIDGET_H
