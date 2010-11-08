CC = $(CROSS_COMPILE)gcc
AS = $(CROSS_COMPILE)as
CFLAGS += -Wall -ggdb
LDFLAGS += -ldl
ifndef DEBUG
CFLAGS += -O2
endif
ARCH ?= arm

TARGET = libSDL-1.2.so.0
OBJS += main.o pmsdl_input.o \
	common/input.o linux/fbdev.o linux/in_evdev.o linux/oshide.o linux/plat.o
ifeq ($(ARCH),arm)
OBJS += arm_utils.o
else
CFLAGS += -fPIC
endif
CFLAGS += -DIN_EVDEV

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -shared -o $@ $^ $(LDFLAGS)

clean:
	$(RM) $(TARGET) $(OBJS)

*.o: pmsdl.h
