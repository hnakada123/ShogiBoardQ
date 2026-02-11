#ifndef SENNICHITEDETECTOR_H
#define SENNICHITEDETECTOR_H

/// @file sennichitedetector.h
/// @brief 千日手検出ユーティリティクラスの定義

#include <QStringList>

/**
 * @brief 千日手（同一局面の4回出現）を検出するユーティリティクラス
 *
 * SFEN履歴から千日手を判定する。通常の千日手（引き分け）と
 * 連続王手の千日手（反則負け）を区別して報告する。
 * 状態を持たないstaticメソッドのみのクラス。
 */
class SennichiteDetector {
public:
    /// 千日手判定の結果
    enum class Result {
        None,                 ///< 千日手ではない
        Draw,                 ///< 千日手（引き分け）
        ContinuousCheckByP1,  ///< 先手の連続王手 → 先手の反則負け
        ContinuousCheckByP2   ///< 後手の連続王手 → 後手の反則負け
    };

    /**
     * @brief sfenRecord の最新局面が4回目の出現かチェックし、結果を返す
     * @param sfenRecord SFEN履歴（各要素: "board turn hand moveNum"）
     * @return 千日手判定結果
     */
    static Result check(const QStringList& sfenRecord);

    /**
     * @brief SFEN文字列から手数フィールドを除いた局面キーを返す
     * @param sfen "board turn hand moveNum" 形式のSFEN文字列
     * @return "board turn hand" 部分（手数を除く）
     */
    static QString positionKey(const QString& sfen);

private:
    /**
     * @brief 連続王手判定（3回目と4回目の出現の間の手順を調べる）
     * @param sfenRecord SFEN履歴
     * @param thirdIdx 3回目の出現インデックス
     * @param fourthIdx 4回目の出現インデックス（= sfenRecord末尾）
     * @return 連続王手があれば該当する Result、なければ Result::Draw
     */
    static Result checkContinuousCheck(const QStringList& sfenRecord,
                                       int thirdIdx,
                                       int fourthIdx);
};

#endif // SENNICHITEDETECTOR_H
