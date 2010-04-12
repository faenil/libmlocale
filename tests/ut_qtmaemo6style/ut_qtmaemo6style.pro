include(../common_top.pri)
TARGET = ut_qtmaemo6style
INCLUDEPATH += $$MSRCDIR/corelib/widgets $$MSRCDIR/corelib/style $$MSRCDIR/../plainqt/style

TEST_SOURCES = \
    $$MSRCDIR/../plainqt/style/qtmaemo6style.cpp \

# unit test and unit
SOURCES += \
    ut_qtmaemo6style.cpp \

# base classes
SOURCES += \


# service classes
SOURCES += \
    $$STUBSDIR/stubbase.cpp \

# unit test and unit
HEADERS += \
    ut_qtmaemo6style.h \

# base classes
HEADERS += \


# service classes
HEADERS += \


include(../common_bot.pri)
