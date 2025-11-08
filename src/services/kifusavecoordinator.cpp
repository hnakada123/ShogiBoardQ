#include "kifusavecoordinator.h"

#include <QFileDialog>
#include <QApplication>
#include <QDir>
#include <QDateTime>

#include "kifuioservice.h"   // writeKifuFile / makeDefaultSaveFileName
#include "playmode.h"

namespace KifuSaveCoordinator {

QString saveViaDialog(QWidget* parent,
                      const QStringList& kifuLines,
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
    if (defaultName.isEmpty() || defaultName == "_vs.kifu")
        defaultName = "untitled.kifu";

    // ダイアログのカレントを実行ディレクトリへ
    QDir::setCurrent(QApplication::applicationDirPath());

    const QString path = QFileDialog::getSaveFileName(
        parent, QObject::tr("Save File"), defaultName, QObject::tr("Kif(*.kifu)"));
    if (path.isEmpty()) return QString();

    QString err;
    if (!KifuIoService::writeKifuFile(path, kifuLines, &err)) {
        if (outError) *outError = err;
        return QString();
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
