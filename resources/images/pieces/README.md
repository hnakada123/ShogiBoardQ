# 駒画像セット

「表示」→「駒の種類」で選択できます。すべて先手・後手各15種類のSVGです。

| フォルダ | メニュー表示 | デザイン |
| --- | --- | --- |
| `standard` | 標準の駒 | 元の駒セット |
| `clear` | 見やすい駒（太字） | 明るい地色と太いゴシック体 |
| `wood` | 木目の駒（明朝） | 琥珀色の木肌と控えめな木目、太い明朝体 |
| `ivory` | 白い駒（ゴシック） | 白地に濃紺のゴシック体 |
| `dark` | 黒い駒（金文字） | 黒地に金色の太字、成駒は明るい赤色 |

追加セットの輪郭・サイズ・枠線の太さは、各 `standard` SVGと同一です。
先手・後手の変換も個別に読み込んで再現します。
成香は「杏」、成桂は「圭」、成銀は「全」の一文字表記です。
文字はパス化されているため、利用者の環境に日本語フォントは不要です。

## 再生成

Qt 6 GuiとNoto CJKフォントを使って生成します。通常のアプリビルドでは不要です。
リポジトリのルートで実行してください。フォントパスは環境に応じて変更します。

```sh
c++ -std=c++17 -fPIC -Wall -Wextra -Wpedantic -Wshadow \
  scripts/generate_clear_pieces.cpp -o /tmp/generate_clear_pieces \
  $(pkg-config --cflags --libs Qt6Gui)

QT_QPA_PLATFORM=offscreen /tmp/generate_clear_pieces \
  /usr/share/fonts/noto-cjk/NotoSerifCJK-Bold.ttc \
  resources/images/pieces/standard resources/images/pieces/wood wood
QT_QPA_PLATFORM=offscreen /tmp/generate_clear_pieces \
  /usr/share/fonts/noto-cjk/NotoSansCJK-Medium.ttc \
  resources/images/pieces/standard resources/images/pieces/ivory ivory
QT_QPA_PLATFORM=offscreen /tmp/generate_clear_pieces \
  /usr/share/fonts/noto-cjk/NotoSansCJK-Bold.ttc \
  resources/images/pieces/standard resources/images/pieces/dark dark
```

従来の太字セットは `clear/README.md` の手順で再生成できます。
Noto CJKのライセンスは同梱の `OFL.txt` を参照してください。
