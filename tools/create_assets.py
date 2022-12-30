#!/usr/bin/python

# Requires Python v3 Interpreter

# Read all the files in 'data', and create assets.inl with all the data.  For
# text file, we try to keep the data as a string, for binary files we store it
# into a uint8_t array.

from PIL import Image
from collections import namedtuple
import numpy as np
import os
import sys
import subprocess

CWD = os.getcwd()
BASE_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.dirname(BASE_DIR)

if CWD != PROJECT_ROOT:
	print("Error: Run the script from project root, i.e.", PROJECT_ROOT)
	sys.exit(-1)

TYPES = {
	"csv":   { "text": True,  "Disabled": False },
	"ini":   { "text": True,  "Disabled": False },
	"glsl":  { "text": True,  "Disabled": False },
	"png":   { "text": False, "Disabled": False },
	"ttf":   { "text": False, "Disabled": False }
}

# Parse Disabled Stuff, --disabled=csv,glsl
for argument in sys.argv:
	if argument.startswith("--disabled="):
		argument = argument.replace("--disabled=", '')
		argument = argument.split(',')
		for item in argument:
			if item in TYPES:
				TYPES[item]["Disabled"] = True

		break;

GROUPS = [
	'fonts', 'icons',
	'palettes', 'shaders',
	'cursors', 'themes'
]

TEMPLATE = '{{\n\t.path = "{path}",\n\t.size = {size},\n\t.data = {data}\n}},\n\n'
File = namedtuple('File', 'path name data size')

HEADER = "/* This file is autogenerated by tools/create_assets.py */"

def list_files(group):
	ret = []
	for root, dirs, files in os.walk("data/%s" % group):
		for f in files:
			if any(f.endswith('.' + x) for x in TYPES):
				ret.append(os.path.join(root, f).replace("\\", "/"))

	return sorted(ret, key=lambda x: x.upper())

def encode_str(data):
	data = data.decode()

	# Just Replace Platform Independent New Line Feeds ( Windows = \r\n, Mac = \r, Unix = \n ) To \n
	data = data.replace('\r\n', '\n').replace('\r', '\n')

	ret = '"'
	for c in data:
		if c == '\n':
			ret += '\\n'
			continue

		if c == '"': c = '\\"'
		if c == '\\': c = '\\\\'
		ret += c

	ret += '"'
	return ret

def encode_bin(data):
	ret = "(const uint8_t[]) {"
	line = ""
	for i, c in enumerate(data):
		line += "{},".format(c)
		if len(line) >= 70 or i == len(data) - 1:
			ret += line
			line = ""

	ret += "}"
	return ret;

def encode_font(fontPath):
	if not os.path.isfile("./tools/font2inl.out"):
		result = subprocess.run(['clang++', 'tools/font2inl.cpp', '-o', 'tools/font2inl.out'], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
		print(result.stdout.decode('utf-8'))
		if not os.path.isfile("./tools/font2inl.out"):
			print("Cannot compile tools/font2inl.cpp for compressing font!")
			sys.exit(1)

	res = subprocess.run(['./tools/font2inl.out', fontPath], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
	result = res.stdout.decode('utf-8').split('\n')
	if (len(result) < 3):
		print("Length of result is less than 3...\n")
		print(res)
		print(res.stdout.decode('utf-8'))
		sys.exit(1)

	ret = "(const unsigned int["
	ret += result[1]
	ret += "]) {"
	ret += result[2]
	ret += "}"
	return ret, result[0]

def encode_img(imgPath):
	ret = "(unsigned char[]) {"
	data = Image.open(imgPath)
	width, height = data.size
	data = data.convert('RGBA').getdata()

	# We Don't Generate Icons For Sizes more than 48 because of file sizes
	if not imgPath.startswith("data/cursors"):
		if not width == 24 or not height == 24:
			return False

	pixelArr = []
	for pixel in data:
		for comp in pixel:
			ret += "0x%0.2X" % comp
			ret += ','

	ret += "}"
	return ret

def create_file(f):
	data = open(f, 'rb').read()
	size = len(data)
	name = f.replace('/', '_').replace('.', '_').replace('-', '_')
	ext = f.split(".")[-1]

	if TYPES[ext]['Disabled']:
		return False;

	if TYPES[ext]['text']:
		size += 1 # So that we NULL terminate the string.
		data = encode_str(data)
	elif ext == 'png':
		data = encode_img(f)
		if not data:
			return False
	elif ext == 'ttf':
		data, size = encode_font(f)
	else:
		data = encode_bin(data)

	return File(f, name, data, size)

for group in GROUPS:
	files = []

	for f in list_files(group):
		file = create_file(f)
		if file:
			files.append(file)

	if not files:
		continue

	out = open("src/assets/%s.inl" % group, "w")

	out.write(HEADER)
	out.write("\n\n")

	for f in files:
		out.write(TEMPLATE.format(**f._asdict()))

	out.write("\n\n")
