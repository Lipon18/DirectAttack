@echo off
setlocal
cargo run --manifest-path "%~dp0Cargo.toml" --release -- build-debug
pause
endlocal