#!/bin/python

import os
import sys

compiler_name = "g++"
options = ""
output_name = "mash"

libs = []
lib_paths = []
includes = []

excludes = {}
if os.name == 'nt':
	excludes["io-linux.cpp"] = True
	includes.extend((
		"C:\\Users\\Jack\\source\\freetype-2.10.2\\include",
		"C:\\Users\\Jack\\source\\glfw-3.3.4.bin.WIN64\\include",
		"C:\\VulkanSDK\\1.2.135.0\\Include"
	))
	libs.extend(
		("freetype", "glfw3", "vulkan-1", "kernel32", "user32", "shell32", "gdi32", "vcruntime", "msvcrt", "msvcprt", "ucrt")
	)
	lib_paths.extend((
		"C:\\Users\\Jack\\source\\freetype-2.10.2\\win64",
		"C:\\Users\\Jack\\source\\glfw-3.3.4.bin.WIN64\\lib-vc2019",
		"C:\\VulkanSDK\\1.2.135.0\\Lib"
	))
	output_name = "mash.exe"
	compiler_name = "clang"
	options += "-Xlinker /NODEFAULTLIB -Xlinker /SUBSYSTEM:windows -Xlinker /ENTRY:mainCRTStartup"
else:
	excludes["io-windows.cpp"] = True
	includes.append("/usr/include/freetype2")
	libs.extend(("freetype", "glfw", "vulkan"))

cpp_list = []
for l in os.listdir("."):
	if os.path.isfile(l) and l[-4:] == ".cpp" and l not in excludes:
		cpp_list.append(l)

include_string = ""
for i in includes:
	include_string += "-I" + i + " "

libs_string = ""
for l in libs:
	libs_string += "-l" + l + " "

lib_paths_string = ""
for l in lib_paths:
	lib_paths_string += "-L" + l + " "

os.system("{0} {1} -std=c++17 {2} {3} {4} {5} -o {6}".format(compiler_name, options, include_string, lib_paths_string, libs_string, " ".join(cpp_list), output_name))
