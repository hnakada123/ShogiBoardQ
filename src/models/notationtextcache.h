#ifndef NOTATIONTEXTCACHE_H
#define NOTATIONTEXTCACHE_H

/// @file notationtextcache.h
/// @brief 表記の設定ごとに作った表示の文字列を覚えておく（思考の表・棋譜解析の結果の表で共通）

#include <QString>
#include <array>
#include <cstddef>

#include "kifupresentation.h"

/**
 * @brief 表の1行の表示の文字列（Kinds 種類 × 移動元をすべて付けるか）を、表記の設定が変わるまで覚える
 *
 * 英語表記の手・読み筋は局面をたどって作るので重い。表の描画や列幅の計算のたびに作り直さないよう、
 * 作った文字列を KifuPresentation::revision ごとに覚え、設定が変わったら捨てて作り直す。
 */
template <std::size_t Kinds>
class NotationTextCache
{
public:
    /// kind の表示（覚えていなければ make() で作る。空の表示も作ったことを覚える）
    template <typename Make>
    const QString& text(int kind, bool fullOrigin, Make make) const
    {
        const int revision = KifuPresentation::revision();
        if (m_revision != revision) {
            for (auto& texts : m_texts) texts = {};
            m_revision = revision;
        }
        QString& text = m_texts[static_cast<size_t>(kind)][fullOrigin ? 1 : 0];
        if (text.isNull()) {
            text = make();
            if (text.isNull()) text = QLatin1String("");
        }
        return text;
    }

private:
    mutable std::array<std::array<QString, 2>, Kinds> m_texts;
    mutable int m_revision = -1;  ///< m_texts を作ったときの表記の設定（KifuPresentation::revision）
};

#endif // NOTATIONTEXTCACHE_H
