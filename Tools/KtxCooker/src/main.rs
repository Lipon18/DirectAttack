use std::env;
use std::path::Path;
use std::process::Command;
use walkdir::WalkDir;

fn main() {
    // 1. Check if the user passed a path as a command-line argument.
    // If not, fall back to the default models directory.
    let args: Vec<String> = env::args().collect();
    let input_dir = if args.len() > 1 {
        args[1].clone()
    } else {
        r"C:\Projects\DirectAttack\DA\Engine\Content\Models".to_string()
    };

    println!("Scanning '{}' for textures...", input_dir);

    for entry in WalkDir::new(&input_dir).into_iter().filter_map(|e| e.ok()) {
        let path = entry.path();
        if path.is_file() {
            if let Some(ext) = path.extension().and_then(|e| e.to_str()) {
                let ext = ext.to_lowercase();
                if ["png", "jpg", "jpeg", "tga"].contains(&ext.as_str()) {
                    process_texture(path);
                }
            }
        }
    }

    println!("Texture cooking complete!");
}

fn process_texture(input_path: &Path) {
    let parent_dir = input_path.parent().unwrap();
    let file_stem = input_path.file_stem().unwrap().to_str().unwrap();
    
    let final_ktx = parent_dir.join(format!("{}.ktx2", file_stem));

    // Check if an up-to-date .ktx2 file already exists
    if final_ktx.exists() {
        if let (Ok(input_meta), Ok(ktx_meta)) = (std::fs::metadata(input_path), std::fs::metadata(&final_ktx)) {
            if let (Ok(input_time), Ok(ktx_time)) = (input_meta.modified(), ktx_meta.modified()) {
                if ktx_time >= input_time {
                    println!("-> Skipping (up-to-date): {:?}", input_path);
                    return;
                }
            }
        }
    }

    let tmp_ktx = parent_dir.join(format!("{}.tmp.ktx2", file_stem));

    // Heuristic detection: Data textures need UNORM
    let is_data_texture = file_stem.contains("_normal")
        || file_stem.contains("_roughness")
        || file_stem.contains("_metallic")
        || file_stem.contains("_metalrough")
        || file_stem.contains("_ao")
        || file_stem.contains("_occlusion");

    let format = if is_data_texture {
        "R8G8B8A8_UNORM"
    } else {
        "R8G8B8A8_SRGB"
    };

    println!("-> Processing: {:?} [Format: {}]", input_path, format);

    // Build command arguments dynamically
    let mut cmd_args = vec![
        "create".to_string(),
        "--format".to_string(),
        format.to_string(),
        "--generate-mipmap".to_string(),
        "--encode".to_string(),
        "uastc".to_string(),
    ];

    if is_data_texture {
        cmd_args.push("--assign-tf".to_string());
        cmd_args.push("linear".to_string());
    }

    cmd_args.push(input_path.to_str().unwrap().to_string());
    cmd_args.push(tmp_ktx.to_str().unwrap().to_string());

    let create_status = Command::new("ktx")
        .args(&cmd_args)
        .status();

    match create_status {
        Ok(status) if status.success() => {
            let transcode_status = Command::new("ktx")
                .args([
                    "transcode",
                    "--target",
                    "bc7",
                    tmp_ktx.to_str().unwrap(),
                    final_ktx.to_str().unwrap(),
                ])
                .status();

            let _ = std::fs::remove_file(&tmp_ktx);

            match transcode_status {
                Ok(ts) if ts.success() => {
                    println!("   Successfully compiled to {:?}", final_ktx);
                }
                _ => eprintln!("   Error: Failed to transcode {}", file_stem),
            }
        }
        _ => eprintln!("   Error: Failed to create KTX2 for {:?}", input_path),
    }
}