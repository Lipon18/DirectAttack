use clap::{Parser, ValueEnum};
use std::fs;
use std::path::{Path, PathBuf};
use std::process::Command;

#[derive(Copy, Clone, PartialEq, Eq, PartialOrd, Ord, ValueEnum, Debug)]
enum Action {
    Clean,
    Generate,
    BuildDebug,
    BuildRelease,
    ClangFormat,
}

#[derive(Parser, Debug)]
#[command(name = "automation", about = "Automation script in Rust")]
struct Cli {
    #[arg(value_enum)]
    action: Action,
}

struct Config {
    build_folder: &'static str,
    cmake_generator: &'static str,
    platform: &'static str,
    fresh: bool,
    clean_first: bool,
    verbose: bool,
    source_dirs: &'static [&'static str],
}

const CONFIG: Config = Config {
    build_folder: "Build", // Isolated nested subfolder
    cmake_generator: "Visual Studio 17 2022",
    platform: "x64",
    fresh: true,
    clean_first: true,
    verbose: false,
    source_dirs: &["Engine/Source", "Testbed/Source"],
};

fn remove_build_folder() -> bool {
    let path = Path::new(CONFIG.build_folder);
    if path.exists() {
        match fs::remove_dir_all(path) {
            Ok(_) => {
                println!("Removed {} folder.", CONFIG.build_folder);
                true
            }
            Err(e) => {
                eprintln!("Failed to remove {}: {}", CONFIG.build_folder, e);
                false
            }
        }
    } else {
        println!("{} folder does not exist.", CONFIG.build_folder);
        true
    }
}

fn generate_project_files() -> bool {
    let build_path = Path::new(CONFIG.build_folder);
    if !build_path.exists() {
        if let Err(e) = fs::create_dir_all(build_path) {
            eprintln!("Failed to create build folder: {}", e);
            return false;
        } 
        println!("Created {} folder.", CONFIG.build_folder);
    }

    let mut cmd = Command::new("cmake");
    cmd.arg("..")
        .arg("-G")
        .arg(CONFIG.cmake_generator)
        .arg("-A")
        .arg(CONFIG.platform);

    if CONFIG.fresh{
        cmd.arg("--fresh");
    }
    if CONFIG.verbose{
        cmd.arg("--verbose");
    }

    println!("Generating project files in directory '{}' ...", CONFIG.build_folder);

    let status = match cmd.current_dir(build_path).status() {
        Ok(s) => s.success(),
        Err(e) => {
            eprintln!("Failed to execute cmake: {}", e);
            false
        }
    };

    if status {
        println!("Project files generated successfully.");
    } else {
        println!("Failed to generate project files.");
    }
    status
}

fn build_project(config_name: &str) -> bool {
    let build_path = Path::new(CONFIG.build_folder);
    if !build_path.exists() {
        println!("{} folder does not exist. Please generate project files first.", CONFIG.build_folder);
        return false;
    }

    let mut cmd = Command::new("cmake");
    cmd.arg("--build")
        .arg(".")
        .arg("--config")
        .arg(config_name);

    if CONFIG.clean_first {
        cmd.arg("--clean-first");
    }
    if CONFIG.verbose {
        cmd.arg("--verbose");
    }

    println!("Building project in {} mode...", config_name);

    let status = match cmd.current_dir(build_path).status() {
        Ok(s) => s.success(),
        Err(e) => {
            eprintln!("Failed to execute cmake build: {}", e);
            false
        }
    };

    if status {
        println!("Project built successfully in {} mode.", config_name);
    } else {
        println!("Failed to build project in {} mode.", config_name);
    }
    status
}

fn collect_source_files(dir: &Path, extensions: &[&str], files: &mut Vec<PathBuf>) {
    if let Ok(entries) = fs::read_dir(dir) {
        for entry in entries.flatten() {
            let path = entry.path();
            if path.is_dir() {
                collect_source_files(&path, extensions, files);
            } else if path.is_file() {
                if let Some(ext) = path.extension().and_then(|e| e.to_str()) {
                    if extensions.contains(&ext.to_lowercase().as_str()) {
                        files.push(path);
                    }
                }
            }
        }
    }
}

fn run_clang_format() -> bool {
    let extensions = ["h", "hpp", "inl", "cpp", "c"];
    let mut format_sources = Vec::new();

    for dir_str in CONFIG.source_dirs {
        let dir = Path::new(dir_str);
        if dir.exists() {
            collect_source_files(dir, &extensions, &mut format_sources);
        }
    }

    if format_sources.is_empty() {
        println!("No source files found in specified directories.");
        return false;
    }

    // Batch files to stay safely under Windows command line length limits
    let batch_size = 50;
    for chunk in format_sources.chunks(batch_size) {
        let mut cmd = Command::new("clang-format");
        cmd.arg("-i").arg("--style=file");
        for file in chunk {
            cmd.arg(file);
        }

        match cmd.status() {
            Ok(status) if status.success() => {}
            _ => {
                eprintln!("Error running clang-format.");
                return false;
            }
        }
    }

    println!("Clang-format applied to {} files.", format_sources.len());
    true
}

fn ensure_release_binary() {
    // If we are currently running the debug build of the automation tool,
    // re-invoke cargo to build the release binary, then re-launch ourselves with release mode!
    let args: Vec<String> = std::env::args().collect();
    let current_exe = match std::env::current_exe() {
        Ok(path) => path,
        Err(_) => return,
    };
    // Check if we are running out of target/debug
    if current_exe.to_string_lossy().contains("target\\debug") {
        println!("Building automation tool in Release mode...");
        let status = Command::new("cargo")
            .arg("build")
            .arg("--release")
            .status();

        if let Ok(s) = status {
            if s.success() {
                // Find path to release executable
                let release_exe = current_exe.to_string_lossy().replace("target\\debug", "target\\release");
                
                // Relaunch ourselves using the release binary
                let relaunch_status = Command::new(release_exe)
                    .args(&args[1..])
                    .status();

                std::process::exit(match relaunch_status {
                    Ok(exit_status) => exit_status.code().unwrap_or(0),
                    Err(_) => 1,
                });
            }
        }
    }
}

fn main() {
    // Automatically anchor working directory to the repository root
    if let Some(manifest_dir) = option_env!("CARGO_MANIFEST_DIR") {
        if let Some(repo_root) = Path::new(manifest_dir).parent() {
            let _ = std::env::set_current_dir(repo_root);
        }
    }

    // Automatically ensures any invocation via cargo run or debug triggers release binary
    ensure_release_binary();

    let cli = Cli::parse();

    let success = match cli.action {
        Action::Clean => remove_build_folder(),
        Action::Generate => generate_project_files(),
        Action::BuildDebug => build_project("Debug"),
        Action::BuildRelease => build_project("Release"),
        Action::ClangFormat => run_clang_format(),
    };

    std::process::exit(if success { 0 } else { 1 });
}