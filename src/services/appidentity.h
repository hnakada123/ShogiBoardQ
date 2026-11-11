#ifndef APPIDENTITY_H
#define APPIDENTITY_H

/// @file appidentity.h
/// @brief このアプリの名前。設定・データ・待受の場所は、ここの名前だけから決める
///
/// アプリ名を変えると、Qt の標準の場所と SHOGIBOARDQ_*_HOME の下のフォルダが変わり、設定・データ・キャッシュが
/// 分かれる。自動化 API のソケットのようにアプリ名によらない場所を使う待受は、ここの名前を使って分ける。
/// P2P対局の実験版（ShogiBoardQ-P2P。開発方針 4章）は、ここを別の名前にして通常版と混ざらないようにしていた。

namespace AppIdentity {

/// Qt のアプリ名（設定・データ・キャッシュのフォルダ名。OS の鍵の保管庫の P2P対局のプレイヤー鍵の項目のサービス名も）
inline constexpr char kApplicationName[] = "ShogiBoardQ";
/// 設定ファイルの名前
inline constexpr char kSettingsFileName[] = "ShogiBoardQ.ini";
/// デスクトップエントリの名前（拡張子なし。Linux の Wayland でアイコンと関連付ける）
inline constexpr char kDesktopFileName[] = "shogiboardq";
/// 実行時のフォルダ（XDG_RUNTIME_DIR の下）と、Windows の名前付きパイプの名前の接頭辞
inline constexpr char kRuntimeName[] = "shogiboardq";

} // namespace AppIdentity

#endif // APPIDENTITY_H
