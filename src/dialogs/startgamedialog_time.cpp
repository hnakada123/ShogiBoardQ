/// @file startgamedialog_time.cpp
/// @brief 対局開始ダイアログ - 時間設定の確認

#include "startgamedialog.h"
#include "timecontrolvalidator.h"

#include <QMessageBox>

bool StartGameDialog::confirmTimeSettings()
{
    using TimeControlValidator::Issue;
    const TimeControlValidator::PlayerTime p1{m_basicTimeHour1, m_basicTimeMinutes1,
                                              m_byoyomiSec1, m_addEachMoveSec1};
    const TimeControlValidator::PlayerTime p2{m_basicTimeHour2, m_basicTimeMinutes2,
                                              m_byoyomiSec2, m_addEachMoveSec2};
    const bool hasEngine = m_isEngine1 || m_isEngine2;
    const Issue issue = TimeControlValidator::validate(p1, p2, hasEngine);

    QString message;
    switch (issue) {
    case Issue::None:
        return true;
    case Issue::Player1HasNoTime:
    case Issue::Player2HasNoTime: {
        const QString side = (issue == Issue::Player1HasNoTime) ? tr("先手／下手") : tr("後手／上手");
        message = hasEngine
            ? tr("%1の持ち時間・秒読み・加算がすべて0秒です。\n"
                 "片方の対局者だけを時間無制限にすることはできません。"
                 "両方の対局者に時間を設定してください。").arg(side)
            : tr("%1の持ち時間・秒読み・加算がすべて0秒です。\n"
                 "片方の対局者だけを時間無制限にすることはできません。"
                 "両方の対局者に時間を設定するか、時間無制限で対局する場合は両方とも0秒にしてください。").arg(side);
        break;
    }
    case Issue::EngineWithoutLimit:
        message = tr("エンジンが参加する対局は、時間無制限（持ち時間・秒読み・加算がすべて0秒）にできません。\n"
                     "エンジンは使える時間を0秒と受け取り、ほとんど考えずに指してしまいます。"
                     "持ち時間・秒読み・加算のいずれかを設定してください。");
        break;
    }
    QMessageBox::warning(this, tr("時間設定"), message);
    return false;
}
