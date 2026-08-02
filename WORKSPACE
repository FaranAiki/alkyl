workspace(name = "alkyl")

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

http_archive(
    name = "rules_rust",
    sha256 = "34f19b16a242a59a7217db57ba4b38d350b91df0f0322250269781f33f6df231",
    urls = ["https://github.com/bazelbuild/rules_rust/releases/download/0.51.0/rules_rust-v0.51.0.tar.gz"],
)

load("@rules_rust//rust:repositories.bzl", "rules_rust_dependencies", "rust_register_toolchains")

rules_rust_dependencies()
rust_register_toolchains(edition = "2021")
