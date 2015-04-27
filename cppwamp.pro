#-------------------------------------------------------------------------------
#               Copyright Butterfly Energy Systems 2014-2015.
#          Distributed under the Boost Software License, Version 1.0.
#             (See accompanying file LICENSE_1_0.txt or copy at
#                   http://www.boost.org/LICENSE_1_0.txt)
#-------------------------------------------------------------------------------

TEMPLATE = subdirs

chatexample.subdir = examples/chat
chatexample.target = chat

futucalcexample.subdir = examples/futucalc
futucalcexample.target = futucalc

SUBDIRS = cppwamp \
          test \
          chatexample \
          futucalcexample

test.depends = cppwamp
chatexample.depends = cppwamp
futucalcexample.depends = cppwamp

OTHER_FILES += \
    CMakeLists.txt \
    examples/CMakeLists.txt \
    Doxyfile.in \
    doc/concepts.dox \
    doc/cppwamp.dox \
    doc/registrations.dox \
    doc/subscriptions.dox
