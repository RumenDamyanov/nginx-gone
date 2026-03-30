# Contributing to nginx-gone

Thanks for your interest in improving nginx-gone! This Nginx module helps manage permanently removed URLs by returning HTTP 410 Gone responses.

## Ways to Help

- **Report bugs**: Include reproduction steps, environment details (Nginx version, OS, module config)
- **Propose enhancements**: Outline use case and provide minimal configuration examples
- **Improve documentation**: Fix clarity, add examples, correct spelling/grammar
- **Add tests**: Edge cases, configuration validation, memory leak detection
- **Performance testing**: Benchmark impact on request processing

## Development Setup

### Prerequisites

- Nginx source code (1.24+ recommended)
- GCC compiler and build tools
- Development libraries: `libpcre3-dev` or `libpcre2-dev`, `zlib1g-dev`
- Git for version control

### Local Development

1. **Clone the repository**:
   ```bash
   git clone https://github.com/RumenDamyanov/nginx-gone.git
   cd nginx-gone
   ```

2. **Build the module locally**:
   ```bash
   ./scripts/local-build.sh
   ```

3. **Or build with Nginx source**:
   ```bash
   wget https://nginx.org/download/nginx-1.27.0.tar.gz
   tar xzf nginx-1.27.0.tar.gz
   cd nginx-1.27.0
   ./configure --add-dynamic-module=../src
   make modules
   ```

4. **Run tests**:
   ```bash
   bash test/basic_test.sh
   bash test/regex_test.sh
   bash test/neg_tests.sh
   ```

### Testing Your Changes

- **Build test**: Ensure module compiles without warnings
- **Configuration test**: Validate directive parsing works correctly
- **Memory test**: Check for leaks with valgrind (if available)
- **Integration test**: Load module in running Nginx instance

## Coding Guidelines

- Follow existing nginx module coding style
- Use nginx memory pools — no `malloc`/`free`
- All functions prefixed with `ngx_http_gone_`
- Test with `nginx -t` before submitting changes
- Keep changes focused and minimal

## Pull Request Process

1. Fork the repository
2. Create a feature branch from `master`
3. Make your changes with clear commit messages
4. Ensure all tests pass
5. Submit a pull request with a description of changes

## Questions?

Open a [discussion](https://github.com/RumenDamyanov/nginx-gone/discussions) or [issue](https://github.com/RumenDamyanov/nginx-gone/issues).
