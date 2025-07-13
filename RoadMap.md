# 🛣️ BinaryRPC Roadmap

Welcome! This is the official development roadmap for BinaryRPC.  
This project reached #1 on Hacker News on July 12, 2025 — thank you all for the incredible feedback. 🙏  
We're currently working on delivering a faster, leaner, and more developer-friendly core.

---

## 🔥 v0.2.0 - Minimal Core Release (Work in Progress)

Goal: Strip down all unnecessary dependencies and provide a clean, production-ready, plug-and-play core.

### ✅ Core Changes
- [ ] Remove `folly`, `glog`, `gflags` dependencies completely
- [ ] Replace `F14Map` with `absl::flat_hash_map`
- [ ] Introduce a minimal RAII-friendly `ThreadPool` implementation
- [ ] Add basic error-handling macros and assertions
- [ ] Clean CMake targets with options like `-DBINARYRPC_BUILD_EXAMPLES=ON`

### 🚧 API & Usability Improvements
- [ ] Simplified Payload API: `payload["key"]` with type inference
- [ ] `payload.has("key")`, `.as<T>()`, `.toJson()` sugar syntax
- [ ] Optional custom handler registration with variadic template helper
- [ ] Single-header `binaryrpc.hpp` version (core only, no QoS)

### 📦 Distribution & Packaging
- [ ] Add `vcpkg.json` manifest to lock dependency versions
- [ ] Package as `binaryrpc-core` and `binaryrpc-full`
- [ ] CMake install targets & `find_package` support

---

## 🎯 v0.3.0 - Transport Abstraction & Raw TCP

Goal: Provide alternative transport backends beyond WebSockets.

- [ ] `TransportInterface` abstraction (WebSocket, Raw TCP, QUIC)
- [ ] Add Raw TCP backend (epoll-based)
- [ ] Graceful fallback from WebSocket to Raw TCP
- [ ] Optional TLS (OpenSSL wrapper, cert pinning support)
- [ ] Start QUIC research (experimental branch)

---

## 📊 v0.4.0 - Observability & Benchmarks

Goal: Empower developers to debug and measure performance easily.

- [ ] Internal latency measurement (per message)
- [ ] Integrated Benchmark Tool (BinaryRPC vs gRPC testbed)
- [ ] CPU and memory usage stats (optional)
- [ ] Build GitHub Actions CI job to run benchmarks on PRs

---

## 🤝 v0.5.0 - Community & Extensibility

Goal: Foster a strong developer community around BinaryRPC.

- [ ] Define public plugin interface (e.g., for logging, auth)
- [ ] Open Discussions for feature voting
- [ ] Add good-first-issue tags to GitHub Issues
- [ ] Publish a “Build Your First Real-time App with BinaryRPC” tutorial
- [ ] Create a Discord/Matrix or GitHub Discussions board

---

## 🌌 Beyond 1.0 (Ideas and Experiments)

- [ ] QUIC Transport Backend (through msquic or ngtcp2)
- [ ] Binary serialization option (e.g., Flatbuffers or custom)
- [ ] Actor-style task scheduling layer (optional)
- [ ] Codegen from `.rpc` schema files (TBD)
- [ ] Language bindings: Python, Rust, Go, etc.

---

## 🧠 Inspiration & Philosophy

BinaryRPC is designed for:
- **Real-time systems**: multiplayer games, dashboards, financial feeds
- **Low-latency environments**: sub-millisecond response time
- **Minimal footprint**: no reflection, no slow abstraction soup
- **Explicit over magic**: if you see it, you can debug it.

Thanks to everyone who supported the project on Hacker News.  
Your feedback directly shapes this roadmap — feel free to open a [Discussion](https://github.com/efecan0/binaryrpc-framework/discussions) or submit an [Issue](https://github.com/efecan0/binaryrpc-framework/issues) if you want to help!

Stay tuned,  
— Efecan
