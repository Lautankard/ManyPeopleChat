#!/usr/bin/env python
# -*- coding: utf-8 -*-
import sys
import os

if __name__ == "__main__":
 	num = 10
 	if (len(sys.argv) > 1): 
 		num = int(sys.argv[1])
 	print(num)
 	for i in range(1, num):
 		os.system("./chatClient/chatClient/client.o " + str(i) + "熊")