@echo off
setlocal
cargo run --manifest-path "%~dp0Cargo.toml" --release -- clean
pause
endlocal