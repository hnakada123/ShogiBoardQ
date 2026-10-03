# 駒のセット

`standard/` に先手・後手各15種類のSVGを収録しています。
淡い木肌、細い縁、控えめな木目、太い明朝体を組み合わせた従来の木目セットを、
画像を変えずに標準の駒として採用しています。
成香は「杏」、成桂は「圭」、成銀は「全」の一文字表記です。
文字はパス化されているため、利用者の環境に日本語フォントは不要です。
表示倍率と影は共通の描画処理で付け、盤上・持駒・ドラッグ中・プレビューで統一します。

「表示」→「対局画面の外観…」の「駒」タブで、標準の駒と30種類のバリエーションを選べます。
初期設定は `standard` です。旧セットの保存済み選択も `standard` に移行します。
木目・色調の20種類は標準セットの外形・字形・大きさ・向きを保ちます。
「戦国文字」は同じ外形・大きさ・向きで、文字と兜を組み合わせた独自の字形を使います。

## バリエーション

| グループ | 名前 | フォルダ |
| --- | --- | --- |
| 虎斑 | 淡虎斑 | `torafu_light/` |
| 虎斑 | 絹虎斑 | `torafu_silk/` |
| 虎斑 | 飴虎斑 | `torafu_amber/` |
| 虎斑 | 紅虎斑 | `torafu_red/` |
| 虎斑 | 山吹虎斑 | `torafu_gold/` |
| 木肌 | 白木 | `wood_pale/` |
| 木肌 | 糸柾 | `wood_straight/` |
| 木肌 | 飴柾 | `wood_amber/` |
| 木肌 | 笹杢 | `wood_bamboo/` |
| 木肌 | 胡桃 | `wood_walnut/` |
| 淡色 | 生成り | `tint_linen/` |
| 淡色 | 薄桜 | `tint_sakura/` |
| 淡色 | 青磁 | `tint_celadon/` |
| 淡色 | 月白 | `tint_moon/` |
| 淡色 | 藤鼠 | `tint_wisteria/` |
| 深色 | 黒檀 | `deep_ebony/` |
| 深色 | 鉄紺 | `deep_navy/` |
| 深色 | 深緑 | `deep_green/` |
| 深色 | 葡萄 | `deep_grape/` |
| 深色 | 墨金 | `deep_gold/` |
| 意匠 | 戦国文字 | `sengoku/` |
| チェス | Facet（木肌） | `chess_facet_wood/` |
| チェス | Facet（白） | `chess_facet_paper/` |
| チェス | Facet（墨） | `chess_facet_slate/` |
| チェス | Atelier（木肌） | `chess_atelier_wood/` |
| チェス | Atelier（白） | `chess_atelier_paper/` |
| チェス | Atelier（墨） | `chess_atelier_slate/` |
| チェス | Ribbon（木肌） | `chess_ribbon_wood/` |
| チェス | Ribbon（白） | `chess_ribbon_paper/` |
| チェス | Ribbon（墨） | `chess_ribbon_slate/` |

`variants.json` に配色と木目の生成パラメーターを保存しています。
標準と合わせて31セット、各30枚（先手・後手各15種類）、計930枚です。

## 戦国文字

`sengoku/` はサンプル画像 `sengoku_kanji.png` の字形をパス化したセットです。
サンプル画像と生成プロンプトはリポジトリ外の `~/Pictures/sengoku_sample/` に移動しています。
王・玉・飛・角・金・銀・桂・香・歩と、と・全・圭・杏・馬・龍を収録します。
駒面は生成り色、文字は濃い墨色、成駒は朱色です。
標準SVGの輪郭と種類別縮尺、先後の回転をそのまま使い、フォントやPNGへの実行時依存はありません。
外観ウィンドウの「すべての駒」または「意匠」から選択できます。

再生成には Python 3、Pillow、potrace が必要です。

```sh
python3 scripts/generate_sengoku_pieces.py \
  --source ~/Pictures/sengoku_sample/sengoku_kanji.png \
  --output /tmp/shogiboardq-sengoku-pieces
```

採用サンプルは内蔵 imagegen で生成したもので、生成プロンプトは
`~/Pictures/sengoku_sample/kanji_generation.txt` に保存しています。フォントファイルは使用していません。

## チェス風の駒

`chess_*` は Facet・Atelier・Ribbon の3案それぞれに木肌・白・墨を用意した9セットです。
五角形・種類別の縮尺・先後の向きは標準SVGと同じです。漢字・アルファベットを載せず、
王冠・城・馬・盾・槍などの絵柄で種類を区別し、成駒は元の絵柄を赤色にします。
王将・玉将は同じ王冠です。動きや成り、持駒のルールは通常の将棋と変わりません。
「表示」→「対局画面の外観…」→「駒」→「チェス」で選択でき、
「おすすめの組み合わせ」から選ぶと盤・駒台・背景も見本の配色になります。
「墨」は濃い青灰色の盤面と明るい駒面を組み合わせたものです。

元画像と承認済みHTMLは `design/chess-shogi/` に保存しています。
`generation.txt` と `pawn-lance-refinement.txt` に画像生成時のプロンプトを記録しています。
SVGの五角形はベクター、絵柄は透明PNGの埋め込みです。成駒の赤色とAtelierの陰影は
書き出し時に確定させ、Qt SVGで未対応のフィルターや外部画像には依存しません。

再生成には Node.js 22以上、Chromium、Python 3を使います。アプリのビルド・実行には不要です。

```sh
node scripts/export_chess_symbols.mjs /tmp/shogi-chess-symbols
python3 scripts/generate_chess_pieces.py /tmp/shogi-chess-symbols
```

## 今後のバリエーション追加

標準セットを基に別フォルダへSVGを生成し、`shogiboardq.qrc` に登録します。
`AppSettings::availablePieceStyles()` と外観カタログ、
`BoardColorPresets` の表示名・配色に新しい種類を追加してください。
`PieceImageProvider` が選択の保存と全盤面への反映を担います。
標準セットのリソース名は `:/pieces/`、追加セットは `:/pieces/<種類>/` です。

## 標準セットの再生成

Qt 6 GuiとNoto Serif CJK Boldフォントを使います。通常のアプリビルドでは不要です。
リポジトリのルートで実行してください。生成先を確認してから標準セットと置き換えます。

```sh
c++ -std=c++17 -fPIC -Wall -Wextra -Wpedantic -Wshadow \
  scripts/generate_pieces.cpp -o /tmp/generate_pieces \
  $(pkg-config --cflags --libs Qt6Gui)

QT_QPA_PLATFORM=offscreen /tmp/generate_pieces \
  /usr/share/fonts/noto-cjk/NotoSerifCJK-Bold.ttc \
  resources/images/pieces/standard /tmp/shogiboardq-standard-pieces
```

Noto CJKのライセンスは同梱の `OFL.txt` を参照してください。

## バリエーションの再生成

Python 3 と PySide6 を使います。アプリのビルド・実行には不要です。
標準セットのSVGを読み、字形・外形を維持して色と木目を生成します。
Qt SVGで木目が外へはみ出さないよう、生成時に輪郭内へ切り抜いたベクターパスを保存します。

```sh
python3 scripts/generate_piece_variants.py --output /tmp/shogiboardq-piece-variants
```

生成先の20フォルダを確認後、このディレクトリの同名フォルダへ反映します。
