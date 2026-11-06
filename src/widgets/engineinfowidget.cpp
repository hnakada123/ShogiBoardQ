/// @file engineinfowidget.cpp
/// @brief 思考の表の上に置く、エンジン1台の情報の帯の実装

#include "engineinfowidget.h"

#include "elidelabel.h"
#include "kifumovedelegate.h"
#include "kifupresentation.h"
#include "movefont.h"
#include "tablestyles.h"
#include "usicommlogmodel.h"

#include <QEvent>
#include <QFontMetrics>
#include <QLabel>
#include <QLocale>
#include <QPainter>
#include <QPalette>
#include <QHideEvent>
#include <QResizeEvent>
#include <QShowEvent>

namespace {
constexpr int kFrame = 1;              // 帯の枠の線の太さ
constexpr int kCellPadding = 5;        // 項目の左右の余白
constexpr int kHeadingGap = 4;         // 項目名と値の間
constexpr int kRowPadding = 8;         // 行の文字の上下の余白の合計
constexpr int kSideMarkGap = 4;        // 手番の印とエンジン名の間
constexpr int kMeasureSlack = 2;       // 測った文字の幅に足す分（端数の倍率で項目名が欠けないように）
constexpr int kMinimumNameWidth = 120; // エンジン名にこれだけ残せなければ折り返す

/// 想定の桁数の数値の見本（数字の幅が一定でない書体でも省略しないよう、一番広い数字にする）
QString numberSample(const QString& pattern, const QFontMetrics& metrics)
{
    QChar widest = QLatin1Char('0');
    const QString digits = QStringLiteral("123456789");
    for (const QChar digit : digits) {
        if (metrics.horizontalAdvance(digit) > metrics.horizontalAdvance(widest)) widest = digit;
    }
    QString sample = pattern;
    return sample.replace(QLatin1Char('9'), widest);
}
} // namespace

bool EngineInfoWidget::isMoveField(int field)
{
    return field == FieldPredicted || field == FieldSearched;
}

EngineInfoWidget::FieldCell& EngineInfoWidget::fieldCell(int field)
{
    return m_fields[static_cast<size_t>(field)];
}

const EngineInfoWidget::FieldCell& EngineInfoWidget::fieldCell(int field) const
{
    return m_fields[static_cast<size_t>(field)];
}

EngineInfoWidget::EngineInfoWidget(QWidget* parent, bool showPredictedMove)
    : QWidget(parent)
    , m_sideMark(new QLabel(this))
    , m_name(new ElideLabel(this))
    , m_showPredictedMove(showPredictedMove)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    QPalette bandPalette = palette();
    bandPalette.setColor(QPalette::WindowText, TableStyles::engineBandText());
    setPalette(bandPalette);

    m_sideMark->setObjectName(QStringLiteral("engineInfoSide"));
    m_sideMark->setAlignment(Qt::AlignCenter);
    m_sideMark->hide();
    m_name->setObjectName(QStringLiteral("engineInfoName"));
    m_name->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_name->setElideMode(Qt::ElideRight);

    createFields();
    applyFonts();
    updateBandLayout();
}

void EngineInfoWidget::createFields()
{
    const struct {
        Field field;
        QString heading;
        const char* objectName;
    } fields[] = {
        {FieldPredicted, tr("予想手"), "engineInfoPredicted"},
        {FieldSearched, tr("探索手"), "engineInfoSearched"},
        {FieldDepth, tr("深さ"), "engineInfoDepth"},
        {FieldNodes, tr("ノード数"), "engineInfoNodes"},
        {FieldNps, tr("探索局面数/秒"), "engineInfoNps"},
        {FieldHash, tr("ハッシュ使用率"), "engineInfoHash"},
    };
    QPalette headingPalette = palette();
    headingPalette.setColor(QPalette::WindowText, TableStyles::engineBandHeading());
    for (const auto& field : fields) {
        FieldCell& cell = fieldCell(field.field);
        cell.heading = new QLabel(field.heading, this);
        cell.heading->setPalette(headingPalette);
        cell.heading->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        cell.value = new ElideLabel(this);
        cell.value->setObjectName(QLatin1String(field.objectName));
        cell.value->setElideMode(Qt::ElideRight);
        cell.value->setAlignment((isMoveField(field.field) ? Qt::AlignLeft : Qt::AlignRight) | Qt::AlignVCenter);
    }
    if (!m_showPredictedMove) {
        fieldCell(FieldPredicted).heading->hide();
        fieldCell(FieldPredicted).value->hide();
    }
}

void EngineInfoWidget::applyFonts()
{
    const QFont bandFont = font();
    m_moveFont = MoveFont::font(bandFont);
    QFont nameFont = bandFont;
    nameFont.setBold(true);
    m_name->setFont(nameFont);
    m_sideMark->setFont(m_moveFont);

    for (int field = 0; field < FieldCount; ++field) {
        const FieldCell& cell = fieldCell(field);
        cell.heading->setFont(bandFont);
        cell.value->setFont(isMoveField(field) ? m_moveFont : bandFont);
    }

    // 通常の値が省略なしで収まる幅にする（値が届いても幅を変えないので、見本で決める）。ラベルは指定した書体の
    // 一部（書体名など）をアプリの設定から受けることがあるので、実際に描く書体で測る
    const QFontMetrics moveMetrics = fieldCell(FieldSearched).value->fontMetrics();
    const QFontMetrics numberMetrics = fieldCell(FieldNodes).value->fontMetrics();
    const QString moveSample = MoveFont::usesJapaneseFont() ? QStringLiteral("▲９九銀不成(99)") : QStringLiteral("▲B8hx2b+");
    const QString samples[FieldCount] = {moveSample, moveSample,
                                         numberSample(QStringLiteral("999/999"), numberMetrics),
                                         numberSample(QStringLiteral("999,999,999,999"), numberMetrics),
                                         numberSample(QStringLiteral("999,999,999"), numberMetrics),
                                         numberSample(QStringLiteral("999%"), numberMetrics)};
    for (int field = 0; field < FieldCount; ++field) {
        FieldCell& cell = fieldCell(field);
        cell.headingWidth = cell.heading->fontMetrics().horizontalAdvance(cell.heading->text()) + kMeasureSlack;
        cell.width = 2 * kCellPadding + cell.headingWidth + kHeadingGap
            + cell.value->fontMetrics().horizontalAdvance(samples[field]) + kMeasureSlack;
    }

    m_sideMarkWidth = m_sideMark->fontMetrics().horizontalAdvance(QStringLiteral("▲")) + kMeasureSlack;
    m_preferredNameWidth = 2 * kCellPadding + m_sideMarkWidth + kSideMarkGap
        + m_name->fontMetrics().horizontalAdvance(QStringLiteral("YaneuraOu NNUE 9.10git 64ZEN2")) + kMeasureSlack;
    m_bookNoteWidth = qMax(0, moveMetrics.horizontalAdvance(KifuPresentation::bookMove(moveSample, 99.9))
                                  - moveMetrics.horizontalAdvance(moveSample));
}

void EngineInfoWidget::refreshFonts()
{
    applyFonts();
    updateBandLayout();
}

int EngineInfoWidget::fieldsWidth(const QList<int>& fields) const
{
    int width = 0;
    for (const int field : fields) width += fieldCell(field).width;
    return width;
}

QList<EngineInfoWidget::Row> EngineInfoWidget::arrangeRows(int bandWidth) const
{
    QList<int> moves;
    if (m_showPredictedMove) moves.append(FieldPredicted);
    moves.append(FieldSearched);
    const QList<int> numbers{FieldDepth, FieldNodes, FieldNps, FieldHash};
    // 隠れている間（ドックのタブの裏など）は幅が決まっていない。折り返した高さを親に求めると、ドックの
    // 最小の高さが膨らむ（下のドックが縮まなくなり、盤を拡大するとウィンドウが伸びる）ので、1行として置く
    if (!isVisible() || kMinimumNameWidth + fieldsWidth(moves) + fieldsWidth(numbers) <= bandWidth)
        return {{true, moves + numbers}};

    // エンジン名と指し手の行と、数値の行に分ける。それでも収まらない項目は次の行へ送る
    QList<Row> rows{{true, {}}};
    int used = kMinimumNameWidth;
    const auto flow = [&](const QList<int>& fields) {
        for (const int field : fields) {
            const Row& row = rows.constLast();
            if ((row.hasName || !row.fields.isEmpty()) && used + fieldCell(field).width > bandWidth) {
                rows.append(Row{});
                used = 0;
            }
            rows.last().fields.append(field);
            used += fieldCell(field).width;
        }
    };
    flow(moves);
    rows.append(Row{});
    used = 0;
    flow(numbers);
    return rows;
}

void EngineInfoWidget::updateBandLayout()
{
    const int rowHeight = KifuMoveDelegate::textHeight(font()) + kRowPadding;
    const int bandWidth = qMax(1, width() - 2 * kFrame);
    m_lines.clear();
    int top = kFrame;
    const QList<Row> rows = arrangeRows(bandWidth);
    for (const Row& row : rows) {
        if (top > kFrame) m_lines.append(QLine(kFrame, top, kFrame + bandWidth - 1, top));
        placeRow(row, QRect(kFrame, top, bandWidth, rowHeight));
        top += rowHeight;
    }
    setFixedHeight(top);
    update();
}

void EngineInfoWidget::placeRow(const Row& row, const QRect& rect)
{
    int x = rect.left();
    int searchedExtra = 0;
    if (row.hasName) {
        int nameWidth = rect.width() - fieldsWidth(row.fields);
        // 余った幅はまずエンジン名に使い、さらに余れば探索手の定跡の表記の分に回す
        // （どちらもドックの幅だけで決まるので、2つのエンジンの帯の項目は揃ったまま）
        if (row.fields.contains(FieldSearched)) {
            searchedExtra = qBound(0, nameWidth - m_preferredNameWidth, m_bookNoteWidth);
            nameWidth -= searchedExtra;
        }
        placeName(QRect(x, rect.top(), qMax(0, nameWidth), rect.height()));
        x += nameWidth;
    }
    // エンジン名の無い行は、項目で行の幅を分け合う
    const auto count = static_cast<int>(row.fields.size());
    int spare = row.hasName || count == 0 ? 0 : rect.width() - fieldsWidth(row.fields);
    for (int i = 0; i < count; ++i) {
        const int field = row.fields.at(i);
        const int share = spare / (count - i);
        spare -= share;
        const int cellWidth = fieldCell(field).width + share + (field == FieldSearched ? searchedExtra : 0);
        if (x > rect.left()) m_lines.append(QLine(x, rect.top(), x, rect.top() + rect.height() - 1));
        placeField(fieldCell(field), QRect(x, rect.top(), cellWidth, rect.height()));
        x += cellWidth;
    }
}

void EngineInfoWidget::placeName(const QRect& rect)
{
    int left = rect.left() + kCellPadding;
    if (m_side) {
        m_sideMark->setFixedSize(m_sideMarkWidth, rect.height());
        m_sideMark->move(left, rect.top());
        left += m_sideMarkWidth + kSideMarkGap;
    }
    m_name->setFixedSize(qMax(0, rect.right() + 1 - kCellPadding - left), rect.height());
    m_name->move(left, rect.top());
}

// 値の更新で親の配置を計算し直さないよう、どの部品も大きさを固定して置く（QLabel の updateGeometry が何もしなくなる）
void EngineInfoWidget::placeField(const FieldCell& cell, const QRect& rect)
{
    const int left = rect.left() + kCellPadding;
    cell.heading->setFixedSize(cell.headingWidth, rect.height());
    cell.heading->move(left, rect.top());
    const int valueLeft = left + cell.headingWidth + kHeadingGap;
    cell.value->setFixedSize(qMax(0, rect.right() + 1 - kCellPadding - valueLeft), rect.height());
    cell.value->move(valueLeft, rect.top());
}

void EngineInfoWidget::setFieldValue(Field field, const QString& value)
{
    fieldCell(field).value->setFullText(value);   // 思考情報は1秒に何度も届く。同じ値なら何もしない
}

void EngineInfoWidget::clearValues()
{
    for (int field = 0; field < FieldCount; ++field) setFieldValue(static_cast<Field>(field), QString());
}

void EngineInfoWidget::setFontSize(int pointSize)
{
    m_fontSize = pointSize;
    QFont bandFont = font();
    bandFont.setPointSize(pointSize);
    setFont(bandFont);   // 測り直しは changeEvent で行う
}

void EngineInfoWidget::setSide(std::optional<Turn> side)
{
    if (m_side == side) return;
    m_side = side;
    m_sideMark->setVisible(side.has_value());
    if (side) {
        const bool black = *side == Turn::Black;
        m_sideMark->setText(black ? QStringLiteral("▲") : QStringLiteral("△"));
        m_sideMark->setAccessibleName(black ? tr("先手") : tr("後手"));
    }
    updateBandLayout();
}

void EngineInfoWidget::setModel(UsiCommLogModel* m)
{
    if (m_model == m) return;
    if (m_model) disconnect(m_model, nullptr, this, nullptr);
    m_model = m;
    if (!m_model) {
        // モデルが無いときは見た目を空に
        m_name->setFullText(QString());
        clearValues();
        return;
    }

    connect(m_model, &UsiCommLogModel::engineNameChanged, this, &EngineInfoWidget::onNameChanged);
    connect(m_model, &UsiCommLogModel::statusChanged, this, &EngineInfoWidget::onStatusChanged);
    onNameChanged();
    onStatusChanged();
}

void EngineInfoWidget::onStatusChanged()
{
    // 表記の設定は実行中に変わる（指し手の書体が変わる）
    if (MoveFont::font(font()) != m_moveFont) refreshFonts();

    // ノード数・NPS は英語の3桁区切り、深さは「深さ/選択的探索の深さ」、ハッシュは百分率
    static const QLocale numbers(QLocale::English);
    const EngineSearchStatus& status = m_model->status();
    QString depth;
    if (status.depth >= 0) {
        depth = QString::number(status.depth);
        if (status.seldepth >= 0) depth += QLatin1Char('/') + QString::number(status.seldepth);
    }
    setFieldValue(FieldPredicted, m_model->predictedMove());
    setFieldValue(FieldSearched, status.searchedMove);
    setFieldValue(FieldDepth, depth);
    setFieldValue(FieldNodes, status.nodes >= 0 ? numbers.toString(status.nodes) : QString());
    setFieldValue(FieldNps, status.nps >= 0 ? numbers.toString(status.nps) : QString());
    setFieldValue(FieldHash, status.hashfull >= 0 ? QString::number(status.hashfull / 10) + QLatin1Char('%') : QString());
}

void EngineInfoWidget::setDisplayNameFallback(const QString& name)
{
    m_fallbackName = name;
    // モデル未設定 or まだ空ならフォールバックを表示
    if (!m_model || m_model->engineName().isEmpty()) m_name->setFullText(m_fallbackName);
}

void EngineInfoWidget::onNameChanged()
{
    const QString name = m_model ? m_model->engineName() : QString();
    m_name->setFullText(name.isEmpty() ? m_fallbackName : name);
}

void EngineInfoWidget::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::FontChange) refreshFonts();
}

void EngineInfoWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    if (event->oldSize().width() != width()) updateBandLayout();
}

void EngineInfoWidget::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    updateBandLayout();
}

void EngineInfoWidget::hideEvent(QHideEvent* event)
{
    QWidget::hideEvent(event);
    updateBandLayout();
}

void EngineInfoWidget::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.fillRect(rect(), TableStyles::engineBandBackground());
    painter.setPen(TableStyles::engineBandLine());
    painter.drawLines(m_lines);
    // 枠（下の線はすぐ下の思考の表の上の線が兼ねる）
    painter.setPen(TableStyles::frameLine());
    const int right = width() - 1;
    const int bottom = height() - 1;
    painter.drawLine(0, 0, right, 0);
    painter.drawLine(0, 0, 0, bottom);
    painter.drawLine(right, 0, right, bottom);
}
