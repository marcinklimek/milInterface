from __future__ import print_function

import serial

import sys
import ctypes
import mil as MIL

startCmd  = b"\02SC0/1/1/4\r\n"

def send(cmd):
	
	print("send")
	print([chr(x) for x in cmd])

	with serial.Serial("COM3", 38400, timeout=1) as port:
		port.write(cmd)
		result = port.readline()
		print(result)


def startGrab():

	print("start grab")
	send(startCmd)

startGrab()