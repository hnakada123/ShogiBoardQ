#ifndef EXECUTABLEDIRECTORY_H
#define EXECUTABLEDIRECTORY_H

/// @file executabledirectory.h
/// @brief 実行ファイルの実体があるフォルダ（翻訳・ライセンス文書・P2P対局の通信プロセスの置き場所）

#include <QString>

/**
 * 実行ファイルの横に置くもの（翻訳の .qm・ライセンス文書・通信プロセス shogiboardq-peer）は、このフォルダから探す。
 *
 * QCoreApplication::applicationDirPath() は、シンボリックリンクから起動するとリンクのあるフォルダを返す（Qt 6.12）。
 * Arch Linux・Ubuntu のパッケージは本体を /usr/lib/shogiboardq に置き、/usr/bin/ShogiBoardQ をリンクにしているので、
 * そこから起動すると /usr/bin を探して、通信プロセスも翻訳も見つからなかった。ここではリンクをたどった先を返す。
 */
namespace ExecutableDirectory {

/// 実行ファイルの実体があるフォルダ（QCoreApplication を作った後に呼ぶ）
QString path();

} // namespace ExecutableDirectory

#endif // EXECUTABLEDIRECTORY_H
