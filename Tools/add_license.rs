use std::{env, fs, io, path::Path};

const COPYRIGHT: &str = "Copyright (c) 2026 YourName";
const EXTS: &[&str] = &["h", "hpp", "inl"]; // headers only, no .cpp / .c
const SKIP_DIRS: &[&str] = &[
    "Build", "build", "out", "ThirdParty", "External", "ExternalDependencies", ".git", ".vs",
];

fn visit(dir: &Path, dry_run: bool) -> io::Result<()> {
    for entry in fs::read_dir(dir)? {
        let path = entry?.path();

        if path.is_dir() {
            let name = path.file_name().and_then(|n| n.to_str()).unwrap_or("");
            if !SKIP_DIRS.contains(&name) {
                visit(&path, dry_run)?;
            }
        } else if path
            .extension()
            .and_then(|e| e.to_str())
            .is_some_and(|e| EXTS.contains(&e))
        {
            if let Err(err) = process(&path, dry_run) {
                eprintln!("skipped {}: {}", path.display(), err);
            }
        }
    }
    Ok(())
}

fn process(path: &Path, dry_run: bool) -> io::Result<()> {
    let text = fs::read_to_string(path)?; // errors on non-UTF-8, caught by caller

    if text.contains("SPDX-License-Identifier") {
        return Ok(());
    }

    // Preserve BOM and the file's existing line endings.
    let (bom, body) = match text.strip_prefix('\u{feff}') {
        Some(rest) => ("\u{feff}", rest),
        None => ("", text.as_str()),
    };
    let eol = if body.contains("\r\n") { "\r\n" } else { "\n" };

    let file_name = path.file_name().and_then(|n| n.to_str()).unwrap_or("");

    let header = format!(
        "/**{eol} * @file {file_name}{eol} * @brief TODO: describe this file.{eol} *{eol} * {COPYRIGHT}{eol} * SPDX-License-Identifier: MIT{eol} */{eol}{eol}"
    );

    println!("{} {}", if dry_run { "would update" } else { "updated" }, path.display());
    if !dry_run {
        fs::write(path, format!("{bom}{header}{body}"))?;
    }
    Ok(())
}

fn main() -> io::Result<()> {
    let dry_run = env::args().any(|a| a == "--dry-run");
    visit(Path::new("."), dry_run)
}