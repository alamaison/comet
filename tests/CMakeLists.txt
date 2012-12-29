#
# Copyright © 2012 Alexander Lamaison
#
# This material is provided "as is", with absolutely no warranty
# expressed or implied. Any use is at your own risk. Permission to
# use or copy this software for any purpose is hereby granted without
# fee, provided the above notices are retained on all copies.
# Permission to modify the code and to distribute modified code is
# granted, provided the above notices are retained, and a notice that
# the code was modified is included with the above copyright notice.
#
# This file is part of Comet version 2.
# https://github.com/alamaison/comet
#

cmake_minimum_required(VERSION 2.4)

project(comet-tests)

include_directories(../include)

set(SOURCES
  test1
  test2
  test3
  test4
  test5
  test6
  test7
  test8
  test9
  test10
  test11
  test12
  test13
  test14
  test15
  test16
  test17
  test18)

add_executable(tests ${SOURCES})

add_test(test_runner tests)