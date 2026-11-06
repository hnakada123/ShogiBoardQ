#ifndef ENGINEINFOWIDGET_H
#define ENGINEINFOWIDGET_H

/// @file engineinfowidget.h
/// @brief 思考の表の上に置く、エンジン1台の情報の帯

#include <QFont>
#include <QLine>
#include <QList>
#include <QString>
#include <QWidget>

#include <array>
#include <optional>

#include "shogitypes.h"

class ElideLabel;
class QLabel;
class UsiCommLogModel;

/**
 * @brief 手番の印・エンジン名と、予想手・探索手・深さ・ノード数などを「項目名 値」で1行に並べる情報の帯
 *
 * 項目の幅は項目名と想定の値の見本（書体）だけで決め、届いた値では変えない。残りの幅はエンジン名に使う。
 * だから同じ幅の帯は、エンジン名の長さや値によらず、どの項目も同じ位置に並ぶ（対局の2つのエンジンの帯が揃う）。
 * 想定より長い値は省略し、全文はツールチップで読める。項目と余白はドックの幅だけで決まる。
 * 1行に収まらない幅では、エンジン名と指し手の行と、数値の行に折り返す。隠れている間は幅が決まっていないので
 * 折り返さない（高さは固定で親の最小の高さになるので、隠れた帯が親を押し広げないように）。
 *
 * 思考情報は1秒に何度も届くので、値の更新で配置を計算しない（値は描き直すだけ）。下の線は持たず、
 * すぐ下に置く思考の表の上の線が兼ねる。
 */
class EngineInfoWidget : public QWidget {
    Q_OBJECT
public:
    explicit EngineInfoWidget(QWidget* parent = nullptr, bool showPredictedMove = true);
    void setModel(UsiCommLogModel* model);
    void setDisplayNameFallback(const QString& name);
    /// このエンジンが指す側（エンジン名の前に ▲・△ を出す）。片方の側を指していない（検討など）なら nullopt
    void setSide(std::optional<Turn> side);

    // フォントサイズ設定
    void setFontSize(int pointSize);
    int fontSize() const { return m_fontSize; }

protected:
    void changeEvent(QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private slots:
    void onNameChanged();
    /// 探索状況・予想手を出す（数値の書式はここで決める）
    void onStatusChanged();

private:
    /// 帯の項目（並びの順）
    enum Field { FieldPredicted, FieldSearched, FieldDepth, FieldNodes, FieldNps, FieldHash, FieldCount };

    /// 項目1つの部品と幅
    struct FieldCell {
        QLabel* heading = nullptr;
        ElideLabel* value = nullptr;
        int headingWidth = 0;
        int width = 0;   ///< 項目全体の幅（項目名と見本の値が収まる幅）
    };

    /// 帯の1行に置くもの
    struct Row {
        bool hasName = false;  ///< エンジン名を置く
        QList<int> fields;     ///< 項目（Field）
    };

    /// 指し手の項目か（指し手の書体で書き、値を左に寄せる）
    static bool isMoveField(int field);
    FieldCell& fieldCell(int field);
    const FieldCell& fieldCell(int field) const;
    void createFields();
    /// 部品に書体を付け、項目の幅を測る
    void applyFonts();
    /// 書体が変わった（測り直して置き直す）
    void refreshFonts();
    void setFieldValue(Field field, const QString& value);
    void clearValues();

    int fieldsWidth(const QList<int>& fields) const;
    /// 幅 bandWidth の帯の行の分け方
    QList<Row> arrangeRows(int bandWidth) const;
    /// 帯の部品を今の幅に置く（値の更新では呼ばない）
    void updateBandLayout();
    void placeRow(const Row& row, const QRect& rect);
    void placeName(const QRect& rect);
    void placeField(const FieldCell& cell, const QRect& rect);

    UsiCommLogModel* m_model = nullptr;
    QLabel* m_sideMark = nullptr;
    ElideLabel* m_name = nullptr;
    std::array<FieldCell, FieldCount> m_fields;
    QString m_fallbackName;
    std::optional<Turn> m_side;
    int m_fontSize = 10;
    bool m_showPredictedMove = true;   // 予想手の項目を出すか
    QFont m_moveFont;                  // 指し手の書体
    int m_sideMarkWidth = 0;
    int m_preferredNameWidth = 0;      // 代表的なエンジン名が省略なしで収まる、手番の印を含む幅
    int m_bookNoteWidth = 0;           // 定跡手の探索手に付く表記（「（定跡 39.9%）」）の幅
    QList<QLine> m_lines;              // 項目と行の区切りの線
};

#endif // ENGINEINFOWIDGET_H
