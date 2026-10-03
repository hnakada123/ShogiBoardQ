# アプリケーションアイコン

| ファイル | 用途 |
| --- | --- |
| `shogiboardq.png` | macOS等のウィンドウとバージョン情報用1024×1024px。角丸・透明背景。絵柄の幅は画像の約80% |
| `shogiboardq.icns` | macOSアプリケーションバンドル用。16/32/128/256/512ptの1x・2x画像を収録（最大1024px） |
| `linux/shogiboardq.png` | Linux用512×512px。小サイズでも分かる角丸と透明背景。絵柄の幅は画像の約97% |
| `windows/shogiboardq.png` | Linux用と同じ角丸512×512px。高DPI表示とICOの生成元 |
| `shogiboardq.ico` | Windows実行ファイルとウィンドウ用。16/20/24/32/40/48/64/96/128/256pxを収録 |

Linux用はQtリソース、CMakeのhicolor/512x512へのインストール、AppImageで共用します。
KDEパネルやWindowsタスクバーでも図柄が小さく見えないよう、Linux・Windows用の透明な余白を
左右それぞれ約1〜2%に抑えています。角の外側も透明で、木目・黒いQ・「将」を維持しています。
角丸は32px表示で半径約4pxとなる大きさにしています。以前の半径約2pxでは、
KDEの小さなアイコン表示で四角く見えたため、実際の検索画面・パネルで確認しています。
Windows用はICOに加えて512pxのPNGもQIconに登録します。
macOS用のPNG・ICNSも同じ図柄と角丸です。Dockでの見た目の大きさを保つため、
macOS用は従来と同じ約80%の幅で図柄を配置しています。

角丸の図柄は内蔵 image_gen で編集した共通画像を使用しています。
Linux用はImageMagickのLanczosフィルターで512×512pxに縮小し、Windows用にもコピーします。
macOS用は同じ生成画像を852×852pxに縮小して1024×1024pxの透明キャンバス中央に配置しています。
ICO・ICNSは各OS向けPNGから生成し、各サイズの透過情報を保持しています。

## ICOの再生成

リポジトリのルートで実行します（ImageMagickが必要）。

```sh
cp resources/icons/linux/shogiboardq.png resources/icons/windows/shogiboardq.png
magick resources/icons/windows/shogiboardq.png -filter Lanczos \
  -define icon:auto-resize=256,128,96,64,48,40,32,24,20,16 \
  resources/icons/shogiboardq.ico
```

## ICNSの再生成

macOSでImageMagickと標準の`iconutil`を使用します。16px・32pxの通常解像度に加え、
Retina用の各サイズも含めます。

```sh
icon_tmp=$(mktemp -d /tmp/shogiboardq-icons.XXXXXX)
iconset_dir="$icon_tmp/shogiboardq.iconset"
mkdir -p "$iconset_dir"
for size in 16 32 128 256 512; do
  magick resources/icons/shogiboardq.png -filter Lanczos -resize "${size}x${size}" \
    "$iconset_dir/icon_${size}x${size}.png"
  retina_size=$((size * 2))
  magick resources/icons/shogiboardq.png -filter Lanczos -resize "${retina_size}x${retina_size}" \
    "$iconset_dir/icon_${size}x${size}@2x.png"
done
iconutil -c icns "$iconset_dir" -o resources/icons/shogiboardq.icns
```

## 共通の角丸画像の生成プロンプト

```text
Use case: precise-object-edit
Asset type: ShogiBoardQ Linux application icon, transparent PNG.
Input image 1 is the EDIT TARGET: the current production icon.
Primary request: Make ONLY the four outside corners of the wooden square MORE VISIBLY ROUNDED. The previous rounding is too small to see in a KDE panel at 32px. Increase the radius to about 14% of the wooden square width (roughly 70 pixels at the reference 512px size, or 140 pixels on a 1024px output). This is a conventional rounded rectangle: long perfectly straight sides joined by circular quarter-arc corners, NOT a circle or squircle.
Composition: Preserve the current centered wooden board occupying about 95% of the square canvas, with about 2.5% transparent margin on each side. Keep the same overall scale and placement. The corners should be unmistakably rounded at 32x32 pixels, about 4 pixels radius at that display size.
Invariants: Preserve all existing artwork exactly: honey-gold wood texture and color, grid, black capital Q and its long tail, the Japanese character "将", all glyph shapes and proportions, internal layout, and straight-on viewpoint. Only clip the four outer corners of the board to the larger radius; do not redraw, restyle, distort, resize, or move the logo.
Transparency: True transparent alpha outside the rounded board. The four removed corner areas must be completely transparent. Smooth clean antialiased edges with no colored flecks, leftover pixels, halo, shadow, glow, or artifacts outside the silhouette.
Avoid: solid or checkerboard background, added text, new border, bevel, perspective, decorative effects, or mockup.
Return the single finished icon.
```
