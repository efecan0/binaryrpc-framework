# Contributing to BinaryRPC

Thank you for your interest in contributing to BinaryRPC! This document provides guidelines and information for contributors.

## 🚀 Quick Start

1. **Fork** the repository
2. **Clone** your fork locally
3. **Create** a feature branch
4. **Make** your changes
5. **Test** your changes thoroughly
6. **Submit** a pull request

## 📋 Development Setup

### Prerequisites

- **C++20** compatible compiler (GCC 10+, Clang 12+, MSVC 2019+)
- **CMake** 3.16 or higher
- **vcpkg** for dependency management
- **Python 3.8+** (for integration tests)

### Building from Source

```bash
# Clone the repository
git clone https://github.com/your-username/binaryrpc.git
cd binaryrpc

# Install dependencies via vcpkg
vcpkg install uwebsockets jwt-cpp cpr nlohmann-json folly glog gflags fmt

# Configure and build
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build . --config Release

# Run tests
ctest --output-on-failure
```

## 🧪 Testing

### Running Tests

```bash
# Unit tests
cd build
ctest -R unit_tests

# Integration tests
cd tests/integration_tests_py
python -m pytest

# Memory leak tests
cd tests/Memory_leak_stress_test_py
python memory_leak_test.py
```

### Writing Tests

- **Unit tests**: Use Catch2 framework in `tests/` directory
- **Integration tests**: Use Python pytest in `tests/integration_tests_py/`
- **Performance tests**: Add benchmarks in `tests/benchmarks/`

## 📝 Code Style

### C++ Guidelines

- Follow **Google C++ Style Guide** with exceptions:
- Use **snake_case** for functions and variables
- Use **PascalCase** for classes and structs
- Use **UPPER_CASE** for constants and macros
- Maximum line length: **120 characters**

### Header Organization

```cpp
// 1. Include guards
#pragma once

// 2. System includes
#include <vector>
#include <string>

// 3. Third-party includes
#include <uwebsockets/App.h>

// 4. Project includes
#include "binaryrpc/core/interfaces/itransport.hpp"

// 5. Namespace
namespace binaryrpc {
    // Your code here
}
```

### Documentation

- Use **Doxygen** comments for public APIs
- Include **examples** in documentation
- Document **exceptions** and error conditions

## 🔧 Architecture Guidelines

### Adding New Features

1. **Interface First**: Define interfaces in `include/binaryrpc/core/interfaces/`
2. **Implementation**: Add implementation in `src/core/`
3. **Tests**: Write comprehensive tests
4. **Documentation**: Update README and add examples

### Plugin Development

```cpp
// Example plugin structure
class MyPlugin : public IPlugin {
public:
    void initialize() override;
    const char* name() const override { return "MyPlugin"; }
    
private:
    // Plugin-specific members
};
```

### Transport Implementation

```cpp
// Example transport structure
class MyTransport : public ITransport {
public:
    void start(uint16_t port) override;
    void stop() override;
    void send(const std::vector<uint8_t>& data) override;
    // ... other interface methods
};
```

## 🐛 Bug Reports

### Before Submitting

1. **Search** existing issues
2. **Reproduce** the bug consistently
3. **Test** with latest master branch
4. **Include** minimal reproduction code

### Bug Report Template

```markdown
**Description**
Brief description of the issue

**Steps to Reproduce**
1. Step 1
2. Step 2
3. Step 3

**Expected Behavior**
What should happen

**Actual Behavior**
What actually happens

**Environment**
- OS: [e.g., Windows 10, Ubuntu 20.04]
- Compiler: [e.g., GCC 11.2, MSVC 2019]
- BinaryRPC Version: [e.g., commit hash]

**Additional Context**
Any other relevant information
```

## 🔄 Pull Request Process

### Before Submitting PR

1. **Rebase** on latest master
2. **Run** all tests locally
3. **Update** documentation if needed
4. **Add** tests for new features
5. **Check** code style compliance

### PR Template

```markdown
**Description**
Brief description of changes

**Type of Change**
- [ ] Bug fix
- [ ] New feature
- [ ] Breaking change
- [ ] Documentation update

**Testing**
- [ ] Unit tests pass
- [ ] Integration tests pass
- [ ] Manual testing completed

**Breaking Changes**
List any breaking changes and migration steps

**Additional Notes**
Any additional information
```

## 📚 Documentation

### API Documentation

- Use **Doxygen** for C++ API documentation
- Include **usage examples** in comments
- Document **thread safety** guarantees
- Specify **exception safety** levels

### User Documentation

- **README.md**: Quick start and overview
- **docs/**: Detailed documentation
- **examples/**: Working code examples
- **CHANGELOG.md**: Version history

## 🏷️ Versioning

BinaryRPC follows **Semantic Versioning** (SemVer):

- **MAJOR**: Breaking changes
- **MINOR**: New features (backward compatible)
- **PATCH**: Bug fixes (backward compatible)

## 🤝 Community

### Communication

- **Issues**: Use GitHub issues for bugs and feature requests
- **Discussions**: Use GitHub Discussions for questions and ideas
- **Code Review**: All PRs require review from maintainers

### Code of Conduct

- Be **respectful** and inclusive
- Focus on **technical merit**
- Help **new contributors**
- Follow **project guidelines**

## 📄 License

By contributing to BinaryRPC, you agree that your contributions will be licensed under the MIT License.

---

Thank you for contributing to BinaryRPC! 🚀 