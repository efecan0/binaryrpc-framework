# Changelog

All notable changes to BinaryRPC will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Initial open source release
- WebSocket transport implementation
- QoS (Quality of Service) system with three levels
- Session management with TTL support
- Plugin system for extensibility
- Middleware chain for request processing
- JWT authentication middleware
- Rate limiting middleware
- Room plugin for group communication
- MsgPack and SimpleText protocol support
- Comprehensive test suite
- CMake build system
- vcpkg integration

### Changed
- N/A

### Deprecated
- N/A

### Removed
- N/A

### Fixed
- N/A

### Security
- N/A

## [1.0.0] - 2026-01-XX

### Added
- Core framework architecture
- Interface-based design for transports, protocols, and plugins
- Session management with automatic cleanup
- RPC context system for request handling
- Framework API for session manipulation
- Handshake inspector system for authentication
- Duplicate filtering for QoS-1 and QoS-2
- Backoff strategies (Linear, Exponential)
- Connection state management
- Offline message queuing
- Generic index system for session queries
- Comprehensive logging system
- Error handling and propagation
- Thread pool integration with Folly
- Memory leak detection and prevention
- Performance optimizations

### Technical Details
- C++20 standard compliance
- Header-only core design
- Zero-copy payload handling
- Thread-safe session operations
- RAII resource management
- Exception safety guarantees
- Cross-platform compatibility (Windows, Linux, macOS)

### Dependencies
- uWebSockets for WebSocket transport
- Folly for concurrent data structures
- JWT-CPP for authentication
- CPR for HTTP client operations
- nlohmann/json for JSON handling
- Google logging (glog) for logging
- Google flags (gflags) for configuration
- fmt for string formatting
- Boost.Thread for threading utilities

---

## Version History

### Version Numbering
- **MAJOR**: Breaking changes in public API
- **MINOR**: New features (backward compatible)
- **PATCH**: Bug fixes and improvements (backward compatible)

### Release Types
- **Alpha**: Early development, unstable
- **Beta**: Feature complete, testing phase
- **RC**: Release candidate, final testing
- **Stable**: Production ready

### Support Policy
- Latest stable version: Full support
- Previous major version: Security fixes only
- Older versions: No support

---

## Migration Guides

### From Pre-1.0 Versions
- Session API has been updated to use ClientIdentity
- Transport interface has been simplified
- Plugin system has been standardized
- QoS options have been reorganized

### Breaking Changes in 1.0.0
- Session constructor now requires ClientIdentity
- Transport callbacks have been updated
- Plugin initialization has changed
- Some deprecated methods have been removed

---

## Contributors

Thank you to all contributors who have helped make BinaryRPC what it is today!

### Core Contributors
- [Your Name] - Initial architecture and implementation
- [Other Contributors] - Various improvements and fixes

### Community Contributors
- [List community contributors here]

---

## Acknowledgments

- **uWebSockets** team for the excellent WebSocket library
- **Facebook Folly** team for the concurrent data structures
- **Catch2** team for the testing framework
- **vcpkg** team for the package manager
- **CMake** team for the build system

---

*This changelog is maintained by the BinaryRPC development team.* 