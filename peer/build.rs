//! ビルドした時刻を埋め込む（実験版のビルドの使用期限。実装仕様 3.1節）
//!
//! SOURCE_DATE_EPOCH があればその値（再現できるビルド）、なければ今の時刻。

fn main() {
    println!("cargo:rerun-if-env-changed=SOURCE_DATE_EPOCH");
    println!("cargo:rerun-if-changed=build.rs");
    let from_env = std::env::var("SOURCE_DATE_EPOCH")
        .ok()
        .and_then(|v| v.trim().parse::<u64>().ok());
    let now = from_env.unwrap_or_else(|| {
        std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .map(|d| d.as_secs())
            .unwrap_or(0)
    });
    println!("cargo:rustc-env=SBQ_PEER_BUILD_UNIX={now}");
}
