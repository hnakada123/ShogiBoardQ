//! ビルドした時刻を埋め込む（ビルドの使用期限の起点。実装仕様 3.1節）
//!
//! CMake から作るときは、CMake が1回だけ決めた起点（SBQ_P2P_BUILD_EPOCH）を使い、対局・UI側（C++）と同じ
//! 期限にする。cargo だけで作るときは SOURCE_DATE_EPOCH（再現できるビルド）、なければ今の時刻。

fn epoch_from(name: &str) -> Option<u64> {
    std::env::var(name).ok().and_then(|v| v.trim().parse::<u64>().ok())
}

fn main() {
    println!("cargo:rerun-if-env-changed=SBQ_P2P_BUILD_EPOCH");
    println!("cargo:rerun-if-env-changed=SOURCE_DATE_EPOCH");
    println!("cargo:rerun-if-changed=build.rs");
    let now = epoch_from("SBQ_P2P_BUILD_EPOCH")
        .or_else(|| epoch_from("SOURCE_DATE_EPOCH"))
        .unwrap_or_else(|| {
            std::time::SystemTime::now()
                .duration_since(std::time::UNIX_EPOCH)
                .map(|d| d.as_secs())
                .unwrap_or(0)
        });
    println!("cargo:rustc-env=SBQ_PEER_BUILD_UNIX={now}");
}
