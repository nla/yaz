#!/usr/bin/tclsh
#
# This file is part of the YAZ toolkit
# Copyright (c) Index Data 1996-2013
# See the file LICENSE for details.
#
#
# Converts a CSV file with SRW diagnostics to C+H file for easy
# maintenance
#

source [lindex $argv 0]/csvtodiag.tcl

csvtodiag [list [lindex $argv 0]/srw.csv diagsrw.c [lindex $argv 0]/../include/yaz/diagsrw.h] srw {}
