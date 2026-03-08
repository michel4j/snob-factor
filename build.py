import os
import subprocess
import sys

def build():
    # Only build if we're not just creating an sdist
    # The build script builds the shared object into snob/_snob.so
    print("Running custom build.py to build the C extension")
    build_script = os.path.join(os.path.dirname(os.path.abspath(__file__)), "build.sh")
    try:
        subprocess.check_call(["bash", build_script])
    except subprocess.CalledProcessError as e:
        print(f"Build failed with {e}")
        sys.exit(1)

if __name__ == "__main__":
    build()
