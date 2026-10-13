TARGET = shar_psp
OBJS = src/main.o src/driving.o
CFLAGS = -O2 -G0 -Wall -Wextra -std=c99
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)
LIBS = -lpspgum -lpspgu -lm
EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = SHAR PSP - Driving Prototype
PSPSDK := $(shell psp-config --pspsdk-path)
ifeq ($(strip $(PSPSDK)),)
$(error PSPSDK not found. Set PSPDEV and add $$PSPDEV/bin to PATH)
endif
include $(PSPSDK)/lib/build.mak
