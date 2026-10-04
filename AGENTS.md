# Build and verification workflow

- Compile and run relevant tests locally before starting an online build or pushing changes for CI verification. GitHub Actions supplements local verification.
- For Windows binaries, prepare a local Windows toolchain or a local MinGW cross-compilation environment first. Explain unavailable target-platform tooling and verification limits before using an online build; do not silently substitute Linux compilation for Windows compilation.
- Use `cmake -S . -B build-local`, `cmake --build build-local`, and `ctest --test-dir build-local --output-on-failure` with the appropriate local Qt toolchain.
- Distinguish compilation, automated tests, startup checks, and real terminal interaction tests in reports. Cross-compilation does not prove Windows runtime behavior.
- Preserve previously released binaries. Give new feature test builds distinct names; publish a new release only within the user's authorized scope.
