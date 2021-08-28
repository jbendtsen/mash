#!/bin/python

import os
import sys

excludes = {}
if os.name == 'nt':
	excludes["io-linux.cpp"] = True
else:
	excludes["io-windows.cpp"] = True

cpp_list = []
for l in os.listdir("."):
	if os.path.isfile(l) and l[-4:] == ".cpp" and l not in excludes:
		cpp_list.append(l)

os.system("g++ -I/usr/include/freetype2 -lfreetype -lglfw -lvulkan {} -o mash".format(" ".join(cpp_list)))
