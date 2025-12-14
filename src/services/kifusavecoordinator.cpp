#include "kifusavecoordinator.h"

#include <QFileDialog>
#include <QApplication>
#include <QDir>
#include <QDateTime>
#include <QFileInfo>

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

QString saveViaDialogWithKi2(QWidget* parent,
                              const QStringList& kifLines,
                              const QStringList& ki2Lines,
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

    // フィルタを作成（KI2形式も選択可能）
    const QString filter = QObject::tr("KIF形式 (*.kifu *.kif);;KI2形式 (*.ki2);;すべてのファイル (*)");

    const QString path = QFileDialog::getSaveFileName(
        parent, QObject::tr("名前を付けて保存"), defaultName, filter);
    if (path.isEmpty()) return QString();

    // 選択されたファイルの拡張子で保存形式を判断
    QString err;
    const QFileInfo fi(path);
    const QString ext = fi.suffix().toLower();

    if (ext == QStringLiteral("ki2")) {
        // KI2形式で保存
        if (!KifuIoService::writeKifuFile(path, ki2Lines, &err)) {
            if (outError) *outError = err;
            return QString();
        }
    } else {
        // KIF形式で保存
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
