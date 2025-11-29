#include "csatosfenconverter.h"
#include "kifdisplayitem.h"

#include <QStringDecoder>         // ← 追加: 文字コード判定/変換に使用
#include "shogimove.h"         // ← もしこの翻訳単位で ShogiMove に触るなら必要

// ---- Board: 平手初期配置 ----
void CsaToSfenConverter::Board::setHirate()
{
    // クリア
    for (int x = 1; x <= 9; ++x) {
        for (int y = 1; y <= 9; ++y) {
            sq[x][y] = Cell{};
        }
    }
    // 白（後手）陣（y=1: L N S G K G S N L）
    Piece back[9] = { KY, KE, GI, KI, OU, KI, GI, KE, KY };
    for (int x = 1; x <= 9; ++x) { sq[x][1].p = back[x-1]; sq[x][1].c = White; }
    // 白：y=2 角(2,2), 飛(8,2)
    sq[2][2] = { KA, White }; sq[8][2] = { HI, White };
    // 白：歩 y=3
    for (int x = 1; x <= 9; ++x) { sq[x][3] = { FU, White }; }

    // 黒：歩 y=7
    for (int x = 1; x <= 9; ++x) { sq[x][7] = { FU, Black }; }
    // 黒：y=8 飛(2,8), 角(8,8)
    sq[2][8] = { HI, Black }; sq[8][8] = { KA, Black };
    // 黒（先手）陣（y=9: L N S G K G S N L）
    for (int x = 1; x <= 9; ++x) { sq[x][9].p = back[x-1]; sq[x][9].c = Black; }
}

// ---- ファイル読込（エンコーディング自動判別：V3は先頭行 'CSA encoding=UTF-8'／無ければSJIS想定）----
bool CsaToSfenConverter::readAllLinesDetectEncoding_(const QString& path, QStringList& outLines, QString* warn)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (warn) *warn += QStringLiteral("Failed to open: %1\n").arg(path);
        return false;
    }

    const QByteArray raw = f.readAll();
    const bool utf8Header = raw.left(128).contains("CSA encoding=UTF-8");

    QString text;

    if (utf8Header) {
        // 明示UTF-8
        QStringDecoder dec(QStringDecoder::Utf8);
        text = dec(raw);
    } else {
        // まず UTF-8 を試す
        QStringDecoder decUtf8(QStringDecoder::Utf8);
        text = decUtf8(raw);
        if (decUtf8.hasError()) {
            // 失敗時は Shift-JIS 系を試す（環境に応じて "Shift-JIS" or "CP932" が有効）
            QStringDecoder decSjis("Shift-JIS");
            if (!decSjis.isValid())
                decSjis = QStringDecoder("CP932");
            if (decSjis.isValid()) {
                text = decSjis(raw);
            } else {
                // 最後にロケール系へフォールバック
                QStringDecoder decSys(QStringDecoder::System);
                text = decSys(raw);
            }
        }
    }

    // 行区切りの正規化
    text.replace("\r\n", "\n");
    text.replace("\r", "\n");
    outLines = text.split('\n', Qt::KeepEmptyParts);
    return true;
}

bool CsaToSfenConverter::isMoveLine_(const QString& s)
{
    // "+7776FU" or "-0055KA" など。先頭が + / - で、後続は英数。
    if (s.isEmpty()) return false;
    const QChar c = s.at(0);
    return (c == QLatin1Char('+') || c == QLatin1Char('-'));
}
bool CsaToSfenConverter::isResultLine_(const QString& s)
{
    return s.startsWith(QLatin1Char('%'));
}
bool CsaToSfenConverter::isMetaLine_(const QString& s)
{
    // V2.2, V3.0, '$EVENT:' 等のメタ
    if (s.isEmpty()) return false;
    const QChar c = s.at(0);
    return (c == QLatin1Char('V') || c == QLatin1Char('$') || c == QLatin1Char('N'));
}
bool CsaToSfenConverter::isCommentLine_(const QString& s)
{
    // "'" 始まり（人向けコメント）と "'*"（プログラム向けコメント＝V3で取り込み）をスキップ
    if (s.isEmpty()) return false;
    return (s.startsWith('\''));
}

// ---- 開始局面解析（PI=平手, '+'/'-'=手番）----
bool CsaToSfenConverter::parseStartPos_(const QStringList& lines, int& idx,
                                        QString& baseSfen, Color& stm, Board& board)
{
    // 既定は平手・先手番
    board.setHirate();
    stm = Black;

    // base SFEN を平手既定に
    // lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL <side> - 1
    auto sfenOfSide = [](Color c)->QString {
        return c == Black ? QStringLiteral(" b - 1")
                          : QStringLiteral(" w - 1");
    };
    const QString boardSfen = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL");

    // PI や 手番記号 を拾う
    bool sawPI = false;
    for (int i = idx; i < lines.size(); ++i) {
        const QString raw = lines.at(i).trimmed();
        if (raw.isEmpty()) continue;
        if (isMetaLine_(raw) || isCommentLine_(raw)) continue;

        if (raw == QLatin1String("PI")) { // 平手
            sawPI = true;
            ++i;
            // 次の '+', '-' が手番指定なら受け取る
            if (i < lines.size()) {
                const QString nxt = lines.at(i).trimmed();
                if (nxt == QLatin1String("+")) { stm = Black; }
                else if (nxt == QLatin1String("-")) { stm = White; }
                else { --i; } // 手番行ではなかった
            }
            idx = i + 1;
            break;
        } else if (raw == QLatin1String("+")) {
            stm = Black;
            idx = i + 1;
            break;
        } else if (raw == QLatin1String("-")) {
            stm = White;
            idx = i + 1;
            break;
        } else if (isMoveLine_(raw) || isResultLine_(raw)) {
            // いきなり指し手・終局が来たら、そのまま開始（手番は既定の先手）
            idx = i;
            break;
        } else {
            // P1～P9 の駒配置や P+ / P- の持ち駒にも将来的に対応可。
            // 本実装では簡易にスキップ（必要ならここで board を構築）
            continue;
        }
    }

    baseSfen = boardSfen + sfenOfSide(stm);
    Q_UNUSED(sawPI);
    return true;
}

// ---- CSAトークンを USI 1手 にして盤も更新 ----
bool CsaToSfenConverter::parseMoveLine_(const QString& line, Color mover, Board& b,
                                        QString& usiMoveOut, QString& prettyOut, QString* warn)
{
    // "+7776FU[,Txx]" → "+7776FU"
    const int comma = line.indexOf(QLatin1Char(','));
    const QString token = (comma >= 0) ? line.left(comma) : line;

    int fx=0, fy=0, tx=0, ty=0;
    Piece after = NO_P;
    if (!parseCsaMoveToken_(token, fx, fy, tx, ty, after)) {
        if (warn) *warn += QStringLiteral("Malformed move token: %1\n").arg(token);
        return false;
    }

    const bool isDrop = (fx == 0 && fy == 0);

    // ---- ここが重要：drop のときも必ず tx/ty を境界チェック ----
    if (!inside_(tx) || !inside_(ty)) {
        if (warn) *warn += QStringLiteral("Destination out of range: %1\n").arg(token);
        return false;
    }

    // 盤上の元の駒
    Piece beforePiece = NO_P;
    bool  beforeProm  = false;
    if (!isDrop) {
        if (!inside_(fx) || !inside_(fy)) {
            if (warn) *warn += QStringLiteral("Source out of range: %1\n").arg(token);
            return false;
        }
        const Cell from = b.sq[fx][fy];
        beforePiece = from.p;
        beforeProm  = isPromotedPiece_(from.p);
        // 元が空でも解析は継続可能だが、必要ならここで検証エラーにしてもよい
    }

    // USI生成
    const QString toSq = toUsiSquare_(tx, ty);
    QString usi;
    if (isDrop) {
        QString usiPiece;
        switch (after) {
        case FU: usiPiece = QStringLiteral("P"); break;
        case KY: usiPiece = QStringLiteral("L"); break;
        case KE: usiPiece = QStringLiteral("N"); break;
        case GI: usiPiece = QStringLiteral("S"); break;
        case KI: usiPiece = QStringLiteral("G"); break;
        case KA: usiPiece = QStringLiteral("B"); break;
        case HI: usiPiece = QStringLiteral("R"); break;
        default: usiPiece = QStringLiteral("P"); break; // 保険
        }
        usi = usiPiece + QStringLiteral("*") + toSq;
    } else {
        const QString fromSq = toUsiSquare_(fx, fy);
        bool promote = false;
        if (isPromotedPiece_(after)) {
            const Piece base = basePieceOf_(after);
            if (!beforeProm && base == basePieceOf_(beforePiece)) {
                promote = true;
            }
        }
        usi = fromSq + toSq + (promote ? QStringLiteral("+") : QString());
    }
    usiMoveOut = usi;

    // pretty
    const QString sideMark = (mover == Black) ? QStringLiteral("▲") : QStringLiteral("△");
    if (isDrop) {
        const QString pj = pieceKanji_(after);
        const QString dest = zenkakuDigit_(tx) + kanjiRank_(ty);
        prettyOut = sideMark + dest + pj + QStringLiteral("打");
    } else {
        const QString pj = pieceKanji_(basePieceOf_(beforePiece));
        const QString dest = zenkakuDigit_(tx) + kanjiRank_(ty);
        prettyOut = sideMark + dest + pj + QStringLiteral("(")
                    + QString::number(fx) + QString::number(fy) + QStringLiteral(")");
        // if (usi.endsWith('+')) prettyOut += QStringLiteral("成");
    }

    // 盤更新
    if (!isDrop) {
        b.sq[tx][ty] = { after, mover };
        b.sq[fx][fy] = Cell{};
    } else {
        b.sq[tx][ty] = { after, mover };
    }
    return true;
}

bool CsaToSfenConverter::parseCsaMoveToken_(const QString& token,
                                            int& fx, int& fy, int& tx, int& ty,
                                            Piece& afterPiece)
{
    // 例: "+7776FU" / "-0055KA"   最小長 7
    if (token.size() < 7) return false;

    const QChar sign = token.at(0);
    if (sign != QLatin1Char('+') && sign != QLatin1Char('-')) return false;

    auto d = [](QChar ch)->int { const int v = ch.digitValue(); return (v >= 0 && v <= 9) ? v : -1; };

    fx = d(token.at(1));
    fy = d(token.at(2));
    tx = d(token.at(3));
    ty = d(token.at(4));

    // to は 1..9 必須
    if (tx < 1 || tx > 9 || ty < 1 || ty > 9) return false;

    const bool isDrop = (fx == 0 && fy == 0);
    if (!isDrop) {
        // from は 1..9
        if (fx < 1 || fx > 9 || fy < 1 || fy > 9) return false;
    }

    // 駒種2文字（大文字想定）。QStringViewで非アロケーション
    const QStringView p = QStringView{token}.mid(5, 2);

    if      (p == u"FU") afterPiece = FU;
    else if (p == u"KY") afterPiece = KY;
    else if (p == u"KE") afterPiece = KE;
    else if (p == u"GI") afterPiece = GI;
    else if (p == u"KI") afterPiece = KI;
    else if (p == u"KA") afterPiece = KA;
    else if (p == u"HI") afterPiece = HI;
    else if (p == u"OU") afterPiece = OU;
    else if (p == u"TO") afterPiece = TO;
    else if (p == u"NY") afterPiece = NY;
    else if (p == u"NK") afterPiece = NK;
    else if (p == u"NG") afterPiece = NG;
    else if (p == u"UM") afterPiece = UM;
    else if (p == u"RY") afterPiece = RY;
    else return false;

    return true;
}

// ---- 文字変換 ----
QChar CsaToSfenConverter::usiRankLetter_(int y)
{
    // 1..9 -> 'a'..'i'（USI/SFEN仕様）
    static const char table[10] = {0,'a','b','c','d','e','f','g','h','i'};
    if (y < 1 || y > 9) return QLatin1Char('a');
    return QLatin1Char(table[y]);
}

QString CsaToSfenConverter::toUsiSquare_(int x, int y)
{
    return QString::number(x) + usiRankLetter_(y);
}

bool CsaToSfenConverter::isPromotedPiece_(Piece p)
{
    return (p == TO || p == NY || p == NK || p == NG || p == UM || p == RY);
}

CsaToSfenConverter::Piece CsaToSfenConverter::basePieceOf_(Piece p)
{
    switch (p) {
    case TO: return FU; case NY: return KY; case NK: return KE; case NG: return GI;
    case UM: return KA; case RY: return HI;
    default: return p;
    }
}

QString CsaToSfenConverter::pieceKanji_(Piece p)
{
    switch (p) {
    case FU: return QStringLiteral("歩");
    case KY: return QStringLiteral("香");
    case KE: return QStringLiteral("桂");
    case GI: return QStringLiteral("銀");
    case KI: return QStringLiteral("金");
    case KA: return QStringLiteral("角");
    case HI: return QStringLiteral("飛");
    case OU: return QStringLiteral("玉");
    case TO: return QStringLiteral("と");
    case NY: return QStringLiteral("成香");
    case NK: return QStringLiteral("成桂");
    case NG: return QStringLiteral("成銀");
    case UM: return QStringLiteral("馬");
    case RY: return QStringLiteral("龍");
    default: return QStringLiteral("歩");
    }
}

QString CsaToSfenConverter::zenkakuDigit_(int d)
{
    // 修正: インデックスを d に合わせるため、0番目をダミーまたは '０' とし、
    // 1〜9番目に '１'〜'９' を配置します。
    static const QChar map[] = {
        QChar(u'０'), // 0: 未使用または0
        QChar(u'１'), // 1
        QChar(u'２'), // 2
        QChar(u'３'), // 3
        QChar(u'４'), // 4
        QChar(u'５'), // 5
        QChar(u'６'), // 6
        QChar(u'７'), // 7
        QChar(u'８'), // 8
        QChar(u'９')  // 9
    };

    // d が 1〜9 の範囲内であれば、そのままインデックスとして使用する
    if (d >= 1 && d <= 9) {
        return QString(map[d]);
    }

    return QString(QChar(u'？'));
}

QString CsaToSfenConverter::kanjiRank_(int y)
{
    static const QChar map[10] = { QChar(), QChar(u'一'), QChar(u'二'), QChar(u'三'), QChar(u'四'),
                                   QChar(u'五'), QChar(u'六'), QChar(u'七'), QChar(u'八'), QChar(u'九') };
    if (y >= 1 && y <= 9) return QString(map[y]);
    return QString(QChar(u'？'));
}

bool CsaToSfenConverter::inside_(int v) { return v >= 1 && v <= 9; }

// ---- エントリポイント ----
bool CsaToSfenConverter::parse(const QString& filePath, KifParseResult& out, QString* warn)
{
    QStringList lines;
    if (!readAllLinesDetectEncoding_(filePath, lines, warn)) return false;

    // out を毎回初期化
    out.mainline.baseSfen.clear();
    out.mainline.usiMoves.clear();
    out.mainline.disp.clear();
    out.variations.clear(); // CSAは分岐を持たない

    // 開始局面と手番
    int idx = 0;
    Color stm = Black;
    Board board; board.setHirate();
    if (!parseStartPos_(lines, idx, out.mainline.baseSfen, stm, board)) {
        if (warn) *warn += QStringLiteral("Failed to parse start position.\n");
        return false;
    }

    // 指し手を読む
    Color turn = stm;
    for (int i = idx; i < lines.size(); ++i) {
        QString s = lines.at(i).trimmed();
        if (s.isEmpty() || isMetaLine_(s) || isCommentLine_(s)) continue;

        // ★ 単独の "+" / "-" は「手番マーカー」なので指し手としては扱わずスキップ
        if (s.size() == 1 && (s[0] == QLatin1Char('+') || s[0] == QLatin1Char('-'))) {
            turn = (s[0] == QLatin1Char('+')) ? Black : White;
            continue;
        }

        if (isResultLine_(s)) {
            // 例: %TORYO
            const QString sideMark = (turn == Black) ? QStringLiteral("▲") : QStringLiteral("△");
            QString pretty;
            if (s.startsWith(QStringLiteral("%TORYO"))) {
                pretty = sideMark + QStringLiteral("投了");
            } else {
                // その他の終局コードはそのまま掲示
                pretty = sideMark + s.mid(1);
            }
            {
                KifDisplayItem di;
                di.prettyMove = pretty;
                out.mainline.disp.append(di);
            }
            break;
        }

        if (isMoveLine_(s)) {
            // 先頭の符号から今回の指し手の手番を決定
            const QChar head = s.at(0);
            const Color mover = (head == QLatin1Char('+')) ? Black : White;

            // USIとprettyを生成（",Txx" 付きでも parseMoveLine_ 側で分離して処理）
            QString usi, pretty;
            if (!parseMoveLine_(s, mover, board, usi, pretty, warn)) {
                if (warn) *warn += QStringLiteral("Failed to parse move line: %1\n").arg(s);
                return false;
            }
            out.mainline.usiMoves.append(usi);

            // disp へ（手番記号付きの整形文字列のみ。時間行はここでは生成しない）
            {
                KifDisplayItem di;
                di.prettyMove = pretty;
                out.mainline.disp.append(di);
            }

            // 手番交代（実際に指した側に合わせて反転）
            turn = (mover == Black) ? White : Black;
            continue;
        }

        // それ以外（P1.., P+ などの局面指定・持ち駒記述・単独Txx等）はスキップ
    }

    return true;
}
