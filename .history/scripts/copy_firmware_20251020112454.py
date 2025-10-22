#!/usr/bin/env python3
"""
Standalone script to copy firmware files
Can be run manually or by build systems
"""

import shutil
import os
import sys
from pathlib import Path

def copy_firmware_files():
    """Copy firmware files from build directory to firmware folder"""
    print("=" * 50)
    print("📦 Copying firmware files to firmware/ folder...")
    
    # Get project directory (script is in scripts/ subdirectory)
    script_dir = Path(__file__).parent
    project_dir = script_dir.parent
    
    # Build directory
    build_dir = project_dir / ".pio" / "build" / "esp32doit-devkit-v1"
    firmware_dir = project_dir / "firmware"
    
    # Ensure firmware directory exists
    firmware_dir.mkdir(exist_ok=True)
    
    # Define source and destination files
    files_to_copy = [
        {
            "source": build_dir / "firmware.bin",
            "dest": firmware_dir / "firmware.bin",
            "name": "Main firmware"
        },
        {
            "source": build_dir / "spiffs.bin", 
            "dest": firmware_dir / "spiffs.bin",
            "name": "SPIFFS filesystem"
        }
    ]
    
    success_count = 0
    
    for file_info in files_to_copy:
        source_file = file_info["source"]
        dest_file = file_info["dest"]
        file_name = file_info["name"]
        
        try:
            if source_file.exists():
                shutil.copy2(source_file, dest_file)
                file_size = dest_file.stat().st_size
                print(f"✅ {file_name}: {source_file.name} → firmware/ ({file_size:,} bytes)")
                success_count += 1
            else:
                print(f"⚠️  {file_name}: {source_file.name} not found")
        
        except Exception as e:
            print(f"❌ Error copying {file_name}: {e}")
    
    if success_count > 0:
        print(f"🎯 Successfully copied {success_count} firmware file(s)")
        print("   Ready for manual flashing or distribution!")
    else:
        print("⚠️  No firmware files were copied")
    
    print("=" * 50)
    return success_count

if __name__ == "__main__":
    success_count = copy_firmware_files()
    sys.exit(0 if success_count > 0 else 1)