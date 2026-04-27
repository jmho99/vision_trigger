#!/bin/bash

/usr/bin/busybox devmem 0x2434040 w 0x4
/usr/bin/busybox devmem 0x2440020 w 0x5
/usr/bin/busybox devmem 0x2430068 w 0x8

