#-------------------------------------------------------------------------------
#               Copyright Butterfly Energy Systems 2014-2015.
#          Distributed under the Boost Software License, Version 1.0.
#             (See accompanying file LICENSE_1_0.txt or copy at
#                   http://www.boost.org/LICENSE_1_0.txt)
#-------------------------------------------------------------------------------

TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
    main.cpp

HEADERS +=

OTHER_FILES += \
    CMakeLists.txt

#Preprocessor macros
#DEFINES +=

#Enable C++11 support
QMAKE_CXXFLAGS += -std=c++11

#Enable support for threads
QMAKE_CXXFLAGS += -pthread

#Stop compiling after N errors
QMAKE_CXXFLAGS += -fmax-errors=3

#Add debugging symbols if in debug configuration
CONFIG(debug, debug|release) {
    QMAKE_CXXFLAGS += -g
    QMAKE_LFLAGS += -g
}

#Add include search directories
INCLUDEPATH += \
    $$PWD/../../cppwamp/include \
    $$PWD/../../ext/boost \
    $$PWD/../../ext/msgpack-c/include

#These are to suppress warnings from library headers
QMAKE_CXXFLAGS += \
    -isystem $$PWD/../../ext/boost \
    -isystem $$PWD/../../ext/msgpack-c/include

#Paths for desktop target
linux-g++ {
    BOOST_LIBS_PATH = $$PWD/../../ext/boost/stage/lib
    deploy.path = $$OUT_PWD/assets
    crossbar.path = $$OUT_PWD/.crossbar
}

#The Boost shared libraries we're using
BOOST_LIBS = coroutine context thread system

#Add the Boost libraries to the linker flags
for(i, BOOST_LIBS) {
    LIBS += -lboost_$${i}
}

#Add linker flags for external library paths
LIBS += -L$$BOOST_LIBS_PATH

#Link other external libraries
LIBS += -pthread

#Assets to deploy with executable
deploy.files += $$files(assets/*)
crossbar.files += $$files(../.crossbar/*)

#Add the deployment files to the install list
INSTALLS += deploy crossbar

DEPENDPATH += \
    $$PWD/../../cppwamp/include/cppwamp \
    $$PWD/../../cppwamp/include/cppwamp/internal \
    $$PWD/../../cppwamp/include/cppwamp/types \
    $$PWD/../../cppwamp/include/cppwamp/types/internal

