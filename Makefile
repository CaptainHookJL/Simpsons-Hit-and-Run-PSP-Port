TARGET = shar_psp
OBJS = src/main.o src/driving.o src/mesh.o src/upstream/vehicle_steering.o
CFLAGS = -O2 -G0 -Wall -Wextra -std=c99 -MMD -MP
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)
LIBS = -lpspgum -lpspgu -lm
EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = SHAR PSP - Driving Prototype
BUILD_PRX = 1
PSP_LARGE_MEMORY = 0
PSPSDK := $(shell psp-config --pspsdk-path)
ifeq ($(strip $(PSPSDK)),)
$(error PSPSDK not found. Set PSPDEV and add $$PSPDEV/bin to PATH)
endif
include $(PSPSDK)/lib/build.mak
-include $(OBJS:.o=.d)
