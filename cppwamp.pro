#-------------------------------------------------------------------------------
#               Copyright Butterfly Energy Systems 2014-2015.
#          Distributed under the Boost Software License, Version 1.0.
#             (See accompanying file LICENSE_1_0.txt or copy at
#                   http://www.boost.org/LICENSE_1_0.txt)
#-------------------------------------------------------------------------------

TEMPLATE = subdirs

chatexample.subdir = examples/chat
chatexample.target = chat

timeservice.subdir = examples/timeservice
timeservice.target = timeservice

timeclient.subdir = examples/timeclient
timeclient.target = timeclient

SUBDIRS = cppwamp \
          test \
          chatexample \
          timeservice \
          timeclient

test.depends = cppwamp
chatexample.depends = cppwamp
timeservice.depends = cppwamp
timeclient.depends = cppwamp

OTHER_FILES += \
    CMakeLists.txt \
    examples/CMakeLists.txt \
    Doxyfile.in \
    doc/concepts.dox \
    doc/cppwamp.dox \
    doc/registrations.dox \
    doc/subscriptions.dox
