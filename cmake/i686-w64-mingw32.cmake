#=============================================================================
# Copyright (C) 2013 Daniel Pfeifer <daniel@pfeifer-mail.de>
# Copyright (C) 2013 Alexander Lamaison <awl03@doc.ic.ac.uk>
#
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at
#   http://www.boost.org/LICENSE_1_0.txt
#=============================================================================

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_C_COMPILER i686-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER i686-w64-mingw32-g++)
set(CMAKE_RC_COMPILER i686-w64-mingw32-windres)

# Setting CMAKE_FIND_ROOT_PATH in the way we do below forces CMake to look
# for other files/packages/libraries etc. ONLY under CMAKE_FIND_ROOT_PATH.
# Usually this is the right thing, because it prevents accidentally finding
# something meant for another platform.  However, you may have other stuff,
# for instance Boost, that you want to use but which you don't want to
# install in /usr/i686-w64-mingw32.  Therefore, we use a variable,
# LOCAL_FIND_ROOT, to allow other paths to be included as the root of a
# search tree.

set(CMAKE_FIND_ROOT_PATH /usr/i686-w64-mingw32 ${LOCAL_FIND_ROOT})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)