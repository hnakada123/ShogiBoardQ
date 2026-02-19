/// @file kifusavecoordinator.cpp
/// @brief 棋譜保存コーディネータの実装

#include "kifusavecoordinator.h"

#include <QFileDialog>
#include <QApplication>
#include <QDir>
#include <QDateTime>
#include <QFileInfo>

#include "kifuioservice.h"   // writeKifuFile / makeDefaultSaveFileName
#include "playmode.h"
#include "settingsservice.h"

namespace KifuSaveCoordinator {

QString saveViaDialogWithUsi(QWidget* parent,
                              const QStringList& kifLines,
                              const QStringList& ki2Lines,
                              const QStringList& csaLines,
                              const QStringList& jkfLines,
                              const QStringList& usenLines,
                              const QStringList& usiLines,
                              PlayMode mode,
                              const QString& human1,
                              const QString& human2,
                              const QString& engine1,
                              const QString& engine2,
                              QString* outError)
{
    // 既定ファイル名を生成
    QString defaultName = KifuIoService::makeDefaultSaveFileName(
        mode, human1, human2, engine1, engine2, QDateTime::currentDateTime());
    if (defaultName.isEmpty() || defaultName.startsWith(QStringLiteral("_")))
        defaultName = "untitled.kifu";

    // 前回保存したディレクトリを復元
    const QString lastDir = SettingsService::lastKifuSaveDirectory();
    if (!lastDir.isEmpty())
        defaultName = QDir(lastDir).filePath(defaultName);

    // フィルタを作成（KIF/KI2/CSA/JKF/USEN/USI形式が選択可能）
    const QString filter = QObject::tr("KIF形式 (*.kifu *.kif);;KI2形式 (*.ki2u *.ki2);;CSA形式 (*.csa);;JKF形式 (*.jkf);;USEN形式 (*.usen);;USI形式 (*.usi);;すべてのファイル (*)");

    const QString path = QFileDialog::getSaveFileName(
        parent, QObject::tr("名前を付けて保存"), defaultName, filter);
    if (path.isEmpty()) return QString();

    SettingsService::setLastKifuSaveDirectory(QFileInfo(path).absolutePath());

    // 選択されたファイルの拡張子で保存形式を判断
    QString err;
    const QFileInfo fi(path);
    const QString ext = fi.suffix().toLower();

    if (ext == QStringLiteral("ki2") || ext == QStringLiteral("ki2u")) {
        // KI2形式で保存
        if (!KifuIoService::writeKifuFile(path, ki2Lines, &err)) {
            if (outError) *outError = err;
            return QString();
        }
    } else if (ext == QStringLiteral("csa")) {
        // CSA形式で保存
        if (!KifuIoService::writeKifuFile(path, csaLines, &err)) {
            if (outError) *outError = err;
            return QString();
        }
    } else if (ext == QStringLiteral("jkf")) {
        // JKF形式で保存
        if (!KifuIoService::writeKifuFile(path, jkfLines, &err)) {
            if (outError) *outError = err;
            return QString();
        }
    } else if (ext == QStringLiteral("usen")) {
        // USEN形式で保存
        if (!KifuIoService::writeKifuFile(path, usenLines, &err)) {
            if (outError) *outError = err;
            return QString();
        }
    } else if (ext == QStringLiteral("usi")) {
        // USI形式で保存
        if (!KifuIoService::writeKifuFile(path, usiLines, &err)) {
            if (outError) *outError = err;
            return QString();
        }
    } else {
        // KIF形式で保存（デフォルト）
        if (!KifuIoService::writeKifuFile(path, kifLines, &err)) {
            if (outError) *outError = err;
            return QString();
        }
    }
    return path;
}

bool overwriteExisting(const QString& path,
                       const QStringList& kifuLines,
                       QString* outError)
{
    QString err;
    const bool ok = KifuIoService::writeKifuFile(path, kifuLines, &err);
    if (!ok && outError) *outError = err;
    return ok;
}

} // namespace KifuSaveCoordinator
