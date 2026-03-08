import os
import subprocess
import sys
import shutil


def build():
    print("Running custom build-package.py to compile the C extension via CMake")
    
    # Define directories
    base_dir = os.path.dirname(os.path.abspath(__file__))
    build_dir = os.path.join(base_dir, "build")
    snob_dir = os.path.join(base_dir, "snob")
    
    # Clean previous build directory if it exists
    if os.path.exists(build_dir):
        shutil.rmtree(build_dir)
        
    os.makedirs(build_dir)
    
    try:
        # Configure CMake
        print("Configuring CMake...")
        subprocess.check_call(["cmake", ".."], cwd=build_dir)
        
        # Build project
        print("Building project...")
        subprocess.check_call(["cmake", "--build", ".", "--config", "Release"], cwd=build_dir)
        
        # Find and copy the built shared library
        # Handles both direct output and MSVC-style Release/ subdir output
        lib_name = "_snob.so" # Note: CMakeLists.txt forces this suffix
        
        source_lib_release = os.path.join(build_dir, "Release", lib_name)
        source_lib_direct = os.path.join(build_dir, lib_name)
        dest_lib = os.path.join(snob_dir, lib_name)
        
        if os.path.isfile(source_lib_release):
            print(f"Copying {source_lib_release} -> {dest_lib}")
            shutil.copy2(source_lib_release, dest_lib)
        elif os.path.isfile(source_lib_direct):
            print(f"Copying {source_lib_direct} -> {dest_lib}")
            shutil.copy2(source_lib_direct, dest_lib)
        else:
            print(f"Error: Could not find compiled library '{lib_name}' in expected build directories.", file=sys.stderr)
            sys.exit(1)
            
        print("Build completed successfully.")
        
    except subprocess.CalledProcessError as e:
        print(f"Build failed during CMake execution: {e}", file=sys.stderr)
        sys.exit(1)
    except Exception as e:
        print(f"Unexpected error during build: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    build()
