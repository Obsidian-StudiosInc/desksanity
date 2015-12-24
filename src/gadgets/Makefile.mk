AM_CPPFLAGS += \
-Isrc/gadgets

module_la_SOURCES += \
src/gadgets/core.c \
src/gadgets/demo.c \
src/gadgets/gadget.h \
src/gadgets/start/start.c \
src/gadgets/clock/clock.c \
src/gadgets/clock/clock.h \
src/gadgets/clock/mod.c \
src/gadgets/clock/time.c
