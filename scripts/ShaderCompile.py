#pragma once

# This script checks if a .cl has been modified since last time this script touched
# Which would mean that the shader needs recompilation

import sys, os, time
from subprocess import call

# Get this script's location
def get_script_path():
    return os.path.dirname(os.path.realpath(sys.argv[0]))

# Get resources dir
def get_shader_path():
    return os.path.join(get_script_path(), "..", "resources")

# Get a file's last modification date
def get_file_mtime(filename):
    moddate = os.stat(filename)
    return moddate[8]

# If the shader's source (.cl) was modified after 
# we generated the last binary file, return true
def check_shader_modified(src, binary):
    src_mtime = get_file_mtime(src)
    binary_mtime = get_file_mtime(binary)
    if (src_mtime > binary_mtime):
        return True
    else:
        return False

src = os.path.join(get_shader_path(), "SimpleCopy.cl")
binary = os.path.join(get_shader_path(), "SimpleCopy.spv")
disassembly = os.path.join(get_shader_path(), "SimpleCopy.spvasm")

clspv_path = os.environ['CLSPV_EXE_PATH']
clspv_exe = os.path.join(clspv_path, "clspv.exe")

if check_shader_modified(src, binary) == True:
    print("Shader src was modified.")
    print("Generating {0} : {1}!".format(binary, "Failed" if call([clspv_exe, src, "-o", binary]) else "Success"))
    print("Generating {0} : {1}!".format(disassembly, "Failed" if call([clspv_exe, "-S", src, "-o", disassembly]) else "Success"))
else:
    print("Shader binary is upto date.")




