import re
import os
import sys
import subprocess

FILE_DEFAULT = 0
FILE_SHADER  = 1

def raw2c(fname):
	if not os.path.exists(fname):
		return None

	with open(fname, "rb") as f:
		data = f.read()

	if len(data) == 0:
		return None

	data_name = re.sub(r'[^0-9A-Za-z]', '_', fname) + "_data"

	out = "unsigned char " + data_name + "[] = {"

	idx = 0
	for b in data:
		if idx % 16 == 0:
			out += "\n\t"
		out += "{},".format(b)
		idx += 1

	out += "\n};\n\n"
	return out

file_list = [
	(FILE_SHADER, "vertex.glsl", "vertex", "vertex.spv"),
	(FILE_SHADER, "fragment.glsl", "fragment", "fragment.spv")
]

content = "#pragma once\n\n"

for file in file_list:
	type = file[0]
	fname = file[1]

	if type == FILE_SHADER:
		stage = file[2]
		spv_name = file[3]

		res = subprocess.run(["glslc", "-fshader-stage=" + stage, "-o", spv_name, fname], text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
		if len(res.stdout) > 0:
			print(res.stdout)
		if res.returncode != 0:
			sys.exit(1)

		fname = spv_name

	array = raw2c(fname)
	if array is None:
		print("Could not load {}".format(fname))
		sys.exit(1)

	content += array

with open("assets.h", "w") as f:
	f.write(content)
