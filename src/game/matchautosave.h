#ifndef MATCHAUTOSAVE_H
#define MATCHAUTOSAVE_H

/// @file matchautosave.h
/// @brief 終局した棋譜の自動保存の指定（ローカル対局・通信対局で同じ）

#include <QString>

/// 終局した棋譜をどこへどの形式で自動保存するか
struct MatchAutoSave {
    bool enabled = false;
    QString dir;                                  ///< 保存先フォルダ
    /// 形式（保存の表 KifuFormats::saveTargets の拡張子）。既定はローカル対局・通信対局とも UTF-8 の KIF
    QString extension = QStringLiteral("kifu");

    /// 保存するか（有効で、保存先がある）
    bool isActive() const { return enabled && !dir.isEmpty(); }
};

#endif // MATCHAUTOSAVE_H
