#
# The MIT License
#
# Copyright (c) 2013-2014 Callaghan Innovation, New Zealand
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to
# deal in the Software without restriction, including without limitation the
# rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
# sell copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
# IN THE SOFTWARE.
#

#!/usr/bin/env python

from sys import argv,exit
from os import environ
from helpers.runSusTests import runSusTests

#______________________________________________________________________
#  Test syntax: ( "folder name", "input file", # processors, "OS", ["flags1","flag2",...])
#  flags: 
#       gpu:                    - run test if machine is gpu enabled
#       no_uda_comparison:      - skip the uda comparisons
#       no_memoryTest:          - skip all memory checks
#       no_restart:             - skip the restart tests
#       no_dbg:                 - skip all debug compilation tests
#       no_opt:                 - skip all optimized compilation tests
#       do_performance_test:    - Run the performance test, log and plot simulation runtime.
#                                 (You cannot perform uda comparsions with this flag set)
#       doesTestRun:            - Checks if a test successfully runs
#       abs_tolerance=[double]  - absolute tolerance used in comparisons
#       rel_tolerance=[double]  - relative tolerance used in comparisons
#       exactComparison         - set absolute/relative tolerance = 0  for uda comparisons
#       startFromCheckpoint     - start test from checkpoint. (/home/csafe-tester/CheckPoints/..../testname.uda.000)
#       sus_options="string"    - Additional command line options for sus command
#
#  Notes: 
#  1) The "folder name" must be the same as input file without the extension.
#  2) If the processors is > 1.0 then an mpirun command will be used
#  3) Performance_tests are not run on a debug build.
#______________________________________________________________________

NIGHTLYTESTS = [   #("massX",                 "massX.ups",                 1,  "Linux", ["exactComparison"]),    \
                   #("guni2dRT",              "guni2dRT.ups",              4,  "Linux", ["exactComparison"]),    \
#                   ("SteadyBurn_2dRT",       "SteadyBurn_2dRT.ups",       4,  "Linux", ["exactComparison"]),    \
#                   ("TBurner_2dRT",          "TBurner_2dRT.ups",          4,  "Linux", ["exactComparison"]),    \
                   #("TRWnoz",                "TRWnoz.ups",                1,  "Linux", ["exactComparison"]),    \
                   #("advect_2L_MI",          "advect_2L_MI.ups",          1,  "Linux", ["exactComparison"]),    \
                   #("explode2D_amr",         "explode2D_amr.ups",         8,  "Linux", ["startFromCheckpoint","no_dbg"]),\
                   #("BurnRate",              "BurnRate.ups",              1.1,"Linux", ["startFromCheckpoint"])#, \
#                   ("DDT1ConvectiveBurning", "DDT1ConvectiveBurning.ups", 1.1,"Linux", ["exactComparison"])
    	       ]

#
#                   ("explode2D_amr",         "explode2D_amr.ups",         8,  "Linux", ["startFromCheckpoint"]),\


LOCALTESTS = [   #("massX",                 "massX.ups",                 1,  "Linux", ["exactComparison"]),   \
                 #("guni2dRT",              "guni2dRT.ups",              4,  "Linux", ["exactComparison"]),   \
#                 ("SteadyBurn_2dRT",       "SteadyBurn_2dRT.ups",       4,  "Linux", ["exactComparison"]),   \
#                 ("TBurner_2dRT",          "TBurner_2dRT.ups",          4,  "Linux", ["exactComparison"]),   \
                 #("TRWnoz",                "TRWnoz.ups",                1,  "Linux", ["exactComparison"]),   \
                 #("advect_2L_MI",          "advect_2L_MI.ups",          1,  "Linux", ["exactComparison"])#,   \
#                 ("DDT1ConvectiveBurning", "DDT1ConvectiveBurning.ups", 1.1,"Linux", ["exactComparison"])
    	       ]

#__________________________________

def getNightlyTests() :
  return NIGHTLYTESTS

def getLocalTests() :
  return LOCALTESTS

#__________________________________

if __name__ == "__main__":

  if environ['WHICH_TESTS'] == "local":
    TESTS = LOCALTESTS
  else:
    TESTS = NIGHTLYTESTS

  result = runSusTests(argv, TESTS, "MPMICE")
  exit( result )
