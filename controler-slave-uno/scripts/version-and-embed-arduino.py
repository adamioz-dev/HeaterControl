Import("env")
import os
import datetime

# output hex array file for nodemcu project
HEX_ARRAY_FILENAME = "AVRHexArray.h"
# handle versioning for uno part 
VERSION_FILE = 'ControlVersion'
VERSION_HEADER = 'ControlVersion.h'
VERSION_UNO_NUMBER = 0

try:
    with open(VERSION_FILE) as FILE:
        VERSION_UNO_NUMBER = FILE.readline()
        VERSION_UNO_NUMBER = int(VERSION_UNO_NUMBER)
        VERSION_UNO_NUMBER = VERSION_UNO_NUMBER + 1
except:
    print('No version file found or incorrect data in it. Starting from 0')
    VERSION_UNO_NUMBER = 0
with open(VERSION_FILE, 'w+') as FILE:
    FILE.write(str(VERSION_UNO_NUMBER))
    print('Build number: {}'.format(str(VERSION_UNO_NUMBER)))

VERSION_NUMBER_STR = str(VERSION_UNO_NUMBER)

HEADER_FILE = """
// AUTO GENERATED FILE, DO NOT EDIT
#ifndef PROJECT_UNO_VERSION
    #define PROJECT_UNO_VERSION {}
#endif
#ifndef PROJECT_BUILD_TIMESTAMP
    #define PROJECT_BUILD_TIMESTAMP "{}"
#endif
""".format(VERSION_NUMBER_STR, datetime.datetime.now())

if os.environ.get('PLATFORMIO_INCLUDE_DIR') is not None:
    VERSION_HEADER = os.environ.get('PLATFORMIO_INCLUDE_DIR') + os.sep + VERSION_HEADER
elif os.path.exists("include"):
    VERSION_HEADER = "include" + os.sep + VERSION_HEADER
else:
    PROJECT_DIR = env.subst("$PROJECT_DIR")
    os.mkdir(PROJECT_DIR + os.sep + "include")
    VERSION_HEADER = "include" + os.sep + VERSION_HEADER

with open(VERSION_HEADER, 'w+') as FILE:
    FILE.write(HEADER_FILE)


# run after hex is created 
def post_program_action(source, target, env):
	print("Running post-build commands...")
	program_path = target[0].get_abspath()
	# check if output exist
	if os.path.exists(program_path):
		print("Program path", program_path)
		convertHex(program_path)
	else:
		print("ERROR! HEX is missing!")

env.AddPostAction("$BUILD_DIR/${PROGNAME}.hex", post_program_action)

def convertHex(program_path):
	print("Creating hex array from hex...")
	elem_cnt = 0

	input = open(program_path, "r")
	bytes = ''
	for line in input.read().split('\n'):
		if line[7:9] == "00":
			line = line[9:-2]
			for i in range(0, len(line), 2):
				bytes += chr(int("%s%s" % (line[i], line[i+1]), 16))

	# write version also to hex array file 
	result = "// GENERATED FILE, DO NOT EDIT!" + "\n"
	result += "#ifndef AVR_HEX_ARRAY_H" + "\n"
	result += "#define AVR_HEX_ARRAY_H" + "\n\n"
	result += "#include <Arduino.h>" + "\n\n"
	result += "#define VERSION_UNO " + VERSION_NUMBER_STR + "\n"
	result += "const byte sketch[] PROGMEM = {\n"
	i = 0
	for byte in bytes:
		result += "0x"
		byte = "%x" % ord(byte)
		if len(byte) == 1:
			byte = "0%s" % byte
		result += byte
		i += 1
		if not i == len(bytes):
			result += ','
		elem_cnt += 1
		if (elem_cnt > 32):
			elem_cnt = 0
			result += '\n'
	result += '};\n'
	result += '#define sketchLength %i' % len(bytes) 
	result += "\n\n"
	result += "#endif //AVR_HEX_ARRAY_H"

	#store in node mcu project (flashing can be done with nodemcu)
	parent_dir = os.path.dirname(env.get("PROJECT_DIR"))
	node_mcu_path = os.path.join(parent_dir, "controler-main-nodemcu")
	node_mcu_path = os.path.join(node_mcu_path, "include")
	print()
	output = os.path.join(node_mcu_path, HEX_ARRAY_FILENAME)
	
	with open(output, "w+") as hex_array_file:
		hex_array_file.write(result)
		hex_array_file.close()

	print("Done:",output)

