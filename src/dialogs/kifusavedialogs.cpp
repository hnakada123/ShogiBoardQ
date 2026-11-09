/// @file kifusavedialogs.cpp
/// @brief 棋譜の保存ダイアログ・未保存の確認の実装

#include "kifusavedialogs.h"
#include "kifufilefilter.h"
#include "unsavedchangesprompt.h"

#include <QFileDialog>
#include <QApplication>
#include <QAbstractButton>
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>

#include "gamesettings.h"

bool KifuSaveDialogs::confirmUtf8Fallback(QWidget* parent, const QString& characters)
{
    QMessageBox box(
        QMessageBox::Warning, tr("Shift_JISで保存できない文字"),
        tr("次の文字はShift_JISで保存できません：%1\n\n"
                    "UTF-8で保存しますか？ファイルの先頭行に「encoding=UTF-8」を記録します。").arg(characters),
        QMessageBox::Save | QMessageBox::Cancel,
        parent);
    box.setTextFormat(Qt::PlainText);   // 文には棋譜の文字が入る
    box.button(QMessageBox::Save)->setText(tr("UTF-8で保存"));
    box.button(QMessageBox::Cancel)->setText(tr("キャンセル"));
    box.setDefaultButton(QMessageBox::Save);
    box.setEscapeButton(QMessageBox::Cancel);
    return box.exec() == QMessageBox::Save;
}

QString KifuSaveDialogs::formatName(KifuFormat format)
{
    return KifuFormats::name(format).toUpper();
}

QString KifuSaveDialogs::lostItemsText(const KifuExport::Loss& loss)
{
    QStringList items;
    if (loss.variations) items << tr("分岐");
    if (loss.times) items << tr("消費時間");
    if (loss.comments) items << tr("コメント");
    if (loss.bookmarks) items << tr("しおり");
    if (!loss.terminals.isEmpty()) {
        QStringList terminals;
        for (const TerminalType type : std::as_const(loss.terminals)) {
            const QString label = TerminalTypes::displayLabel(type);
            terminals << (label.isEmpty() ? tr("独自の終局理由") : label);
        }
        items << tr("終局理由（%1）").arg(terminals.join(tr("・")));
    }
    return items.join(tr("、"));
}

bool KifuSaveDialogs::confirmLossySave(QWidget* parent, KifuFormat format, const KifuExport::Loss& loss)
{
    const QString name = formatName(format);
    QMessageBox box(QMessageBox::Warning, tr("%1形式で保存").arg(name),
                    tr("%1形式は次の情報に対応していないため、これらは保存されません：\n%2\n\n保存を続けますか？")
                        .arg(name, lostItemsText(loss)),
                    QMessageBox::Ok | QMessageBox::Cancel, parent);
    box.setTextFormat(Qt::PlainText);
    box.setDefaultButton(QMessageBox::Ok);
    return box.exec() == QMessageBox::Ok;
}

QString KifuSaveDialogs::saveViaDialog(QWidget* parent,
                      const KifuSaveFile::LineGenerator& generateLines,
                      const QString& defaultFileName,
                      const LossOf& lossOf,
                      QString* outError)
{
    // 既定ファイル名を生成
    if (outError) outError->clear();
    QString defaultName = defaultFileName;

    // 前回保存したディレクトリを復元
    const QString lastDir = GameSettings::lastKifuSaveDirectory();
    if (!lastDir.isEmpty())
        defaultName = QDir(lastDir).filePath(defaultName);

    // 絞り込みは保存できる拡張子の表から作る
    QString selectedFilter;
    const QString chosenPath = QFileDialog::getSaveFileName(
        parent, tr("名前を付けて保存"), defaultName, KifuFileFilter::forSaving(), &selectedFilter);
    const QString path = KifuFileFilter::pathForSaving(chosenPath, selectedFilter);
    if (path.isEmpty()) return QString();

    GameSettings::lastKifuSaveDirectory.set(QFileInfo(path).absolutePath());

    // 選択されたファイルの拡張子で保存形式を判断し、その形式に書けないものがあれば確かめる
    const KifuFormat format = KifuSaveFile::saveFormatForPath(path);
    if (lossOf) {
        const KifuExport::Loss loss = lossOf(format);
        if (!loss.isEmpty() && !confirmLossySave(parent, format, loss)) return QString();
    }

    if (!overwriteExisting(parent, path, generateLines, outError)) return QString();
    return path;
}

bool KifuSaveDialogs::confirmDiscardUnsaved(QWidget* parent, bool isDirty, const std::function<bool()>& save)
{
    if (!isDirty) return true;
    switch (UnsavedChangesPrompt::ask(parent, tr("未保存の棋譜"), tr("棋譜に未保存の変更があります。保存しますか？"))) {
    case UnsavedChangesPrompt::Choice::Save:
        return save();
    case UnsavedChangesPrompt::Choice::Discard:
        return true;
    case UnsavedChangesPrompt::Choice::Cancel:
        break;
    }
    return false;
}

bool KifuSaveDialogs::overwriteExisting(QWidget* parent, const QString& path,
                                        const KifuSaveFile::LineGenerator& generateLines, QString* outError)
{
    // 親ウィンドウが無い（自動化 API から保存する）ときは確認を出さずエラーにする
    if (!parent) return KifuSaveFile::write(path, generateLines, outError);
    return KifuSaveFile::write(path, generateLines, outError, [parent](const QString& missing) {
        return confirmUtf8Fallback(parent, missing);
    });
}

