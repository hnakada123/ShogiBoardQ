/// @file dialoglauncher.cpp
/// @brief メニューから開くダイアログの起動の実装

#include "dialoglauncher.h"
#include "notificationpresenter.h"
#include "piecesoundplayer.h"
#include "piecesoundsettingsdialog.h"

#include <QDesktopServices>
#include <QUrl>
#include <utility>

#include "boardappearancedialog.h"
#include "fontsettingsdialog.h"
#include "engineregistrationdialog.h"
#include "evaluationchartsettingsdialog.h"
#include "evaluationchartwidget.h"
#include "promotedialog.h"
#include "shogiview.h"
#include "jishogiscoredialog.h"
#include "shogiboard.h"
#include "tsumeshogigeneratordialog.h"
#include "tsumecollectiondialog.h"
#include "versiondialog.h"
#include "logcategories.h"

DialogLauncher::DialogLauncher(const Deps& deps, QObject* parent)
    : QObject(parent)
    , m_deps(deps)
{
}

void DialogLauncher::displayVersionInformation()
{
    VersionDialog dialog(m_deps.parentWidget);
    dialog.exec();
}

void DialogLauncher::openProjectWebsite()
{
    QDesktopServices::openUrl(QUrl(QStringLiteral("https://hnakada123.github.io/ShogiBoardQ/")));
}

void DialogLauncher::displayEngineSettingsDialog()
{
    EngineRegistrationDialog dialog(m_deps.parentWidget);
    dialog.exec();
    emit engineListChanged();
}

bool DialogLauncher::choosePromotion()
{
    return PromoteDialog::ask(m_deps.parentWidget);
}

void DialogLauncher::displayJishogiScoreDialog()
{
    auto* sv = m_deps.shogiView;
    if (!sv || !sv->board()) {
        NotificationPresenter::warning(m_deps.parentWidget, tr("エラー"), tr("盤面データがありません。"));
        return;
    }

    JishogiScoreDialog::showFor(sv->board()->position(), m_deps.parentWidget);
}

void DialogLauncher::displayTsumePlayDialog()
{
    TsumeCollectionDialog dialog(m_deps.parentWidget);
    // 詰将棋対局の指し手でも、本譜の対局と同じ駒音を鳴らす（オフなら PieceSoundPlayer 側で鳴らない）
    if (auto* player = m_deps.pieceSoundPlayer)
        connect(&dialog, &TsumeCollectionDialog::movePlayed, player, &PieceSoundPlayer::playMoveSound);
    dialog.exec();
}

void DialogLauncher::displayTsumeshogiGeneratorDialog()
{
    TsumeshogiGeneratorDialog dialog(m_deps.parentWidget);
    dialog.exec();
}

void DialogLauncher::displayPieceSoundSettingsDialog()
{
    auto* player = m_deps.pieceSoundPlayer;
    if (!player) return;

    // 変えた値はすぐに駒音に反映して試聴する（キャンセルでは開いたときの値が届く）
    PieceSoundSettingsDialog dialog(player->volume(), player->tone(), m_deps.parentWidget);
    connect(&dialog, &PieceSoundSettingsDialog::volumeChanged, player, &PieceSoundPlayer::setVolume);
    connect(&dialog, &PieceSoundSettingsDialog::toneChanged, player, &PieceSoundPlayer::setTone);
    connect(&dialog, &PieceSoundSettingsDialog::previewRequested, player, &PieceSoundPlayer::preview);
    dialog.exec();
}

void DialogLauncher::displayBoardAppearanceDialog()
{
    if (!m_appearanceDialog) {
        m_appearanceDialog = new BoardAppearanceDialog(m_deps.parentWidget);
        m_appearanceDialog->setAttribute(Qt::WA_DeleteOnClose);
    }
    m_appearanceDialog->show();
    m_appearanceDialog->raise();
    m_appearanceDialog->activateWindow();
}

void DialogLauncher::displayFontSettingsDialog()
{
    FontSettingsDialog dialog(m_deps.parentWidget);
    dialog.exec();
}

void DialogLauncher::displayEvaluationChartSettings()
{
    EvaluationChartWidget* chart = m_deps.evalChart;
    if (!chart) return;
    EvaluationChartSettingsDialog dialog(chart->chartSettings(), chart->axes(), chart);
    if (dialog.exec() == QDialog::Accepted) chart->setChartSettings(dialog.settings());
}
