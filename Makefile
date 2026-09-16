TARGET = JumpQuest
OBJS = source/main.o

INCDIR =
CFLAGS = -O2 -G0 -Wall
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

LIBDIR =
LIBS = -lpspgum -lpspgu -lpspdebug -lpspdisplay -lpspge -lpspctrl -lpspkernel -lm

BUILD_PRX = 1
PSP_FW_VERSION = 500

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = Jump Quest

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
