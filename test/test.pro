#-------------------------------------------------------------------------------
#               Copyright Butterfly Energy Systems 2014-2015.
#          Distributed under the Boost Software License, Version 1.0.
#             (See accompanying file LICENSE_1_0.txt or copy at
#                   http://www.boost.org/LICENSE_1_0.txt)
#-------------------------------------------------------------------------------

TEMPLATE = app
CONFIG += console precompile_header
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
    codectestjson.cpp \
    codectestmsgpack.cpp \
    legacytransporttest.cpp \
    payloadtest.cpp \
    transporttest.cpp \
    varianttestassign.cpp \
    varianttestbadaccess.cpp \
    varianttestcomparison.cpp \
    varianttestconvert.cpp \
    varianttestinfo.cpp \
    varianttestinit.cpp \
    varianttestmap.cpp \
    varianttestoutput.cpp \
    varianttesttuple.cpp \
    varianttestvector.cpp \
    varianttestvisitation.cpp \
    wamptest.cpp \
    wamptestadvanced.cpp \
    main.cpp

HEADERS += \
    precompiled.hpp \
    faketransport.hpp \
    transporttest.hpp

OTHER_FILES += \
    CMakeLists.txt

PRECOMPILED_HEADER = precompiled.hpp

#Preprocessor macros
DEFINES += CPPWAMP_COMPILED_LIB
DEFINES += CPPWAMP_UNIT_TESTING
DEFINES += CPPWAMP_TESTING_VARIANT=1
DEFINES += CPPWAMP_TESTING_CODEC=1
DEFINES += CPPWAMP_TESTING_TRANSPORT=1
DEFINES += CPPWAMP_TESTING_WAMP=1

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
    $$PWD/../cppwamp/include \
    $$PWD/../ext/boost \
    $$PWD/../ext/Catch/include \
    $$PWD/../ext/msgpack-c/include \
    $$PWD/../ext/rapidjson/include

#These are to suppress warnings from library headers
QMAKE_CXXFLAGS += \
    -isystem $$PWD/../ext/boost \
    -isystem $$PWD/../ext/msgpack-c/include

#This will make the executable search for shared libraries under ./libs
QMAKE_LFLAGS += '-Wl,-rpath,\'\$$ORIGIN/libs\''

#Paths for desktop target
linux-g++ {
    BOOST_LIBS_PATH = $$PWD/../ext/boost/stage/lib
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

#Test assets to deploy with executable
deploy.files += $$files(assets/*)
crossbar.files += $$files(./.crossbar/*)

#Add the deployment files to the install list
INSTALLS += deploy crossbar

DEPENDPATH += \
    $$PWD/../cppwamp/include/cppwamp \
    $$PWD/../cppwamp/include/cppwamp/internal

#Add linker flags for cppwamp dependency
win32:CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/../cppwamp/release/ -lcppwamp
else:win32:CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/../cppwamp/debug/ -lcppwamp
else:unix: LIBS += -L$$OUT_PWD/../cppwamp/ -lcppwamp
