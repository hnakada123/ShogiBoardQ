# 見やすい駒（太字）

明るい無地の駒に、太い一文字を大きく配置した駒セットです。
成駒は濃い赤字で、成香は「杏」、成桂は「圭」、成銀は「全」と1文字で表示します。
先手・後手それぞれ15種類（王・玉を含む）のSVGを収録しています。

文字は Noto Sans CJK JP Bold の輪郭をパスに変換しているため、
利用者の環境に日本語フォントがなくても同じ表示になります。
フォントのライセンスは同梱の `OFL.txt` を参照してください。

生成には Qt 6 Gui と Noto Sans CJK Bold が必要です。アプリの通常のビルド時には不要です。
リポジトリのルートで次のコマンドを実行すると再生成できます。

```sh
c++ -std=c++17 -Wall -Wextra -Wpedantic -Wshadow \
  scripts/generate_clear_pieces.cpp -o /tmp/generate_clear_pieces \
  $(pkg-config --cflags --libs Qt6Gui)
QT_QPA_PLATFORM=offscreen /tmp/generate_clear_pieces \
  /usr/share/fonts/noto-cjk/NotoSansCJK-Bold.ttc resources/images/pieces/clear
```

フォントファイルのパスは環境に応じて変更してください。
