import subprocess
import os

os.makedirs("build", exist_ok=True)
os.chdir("build")
subprocess.run(["cmake", ".."])
subprocess.run(["make"])

