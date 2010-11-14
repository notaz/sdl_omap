CC = $(CROSS_COMPILE)gcc
AS = $(CROSS_COMPILE)as
CFLAGS += -Wall -ggdb
LDFLAGS += -ldl
ifndef DEBUG
CFLAGS += -O2
endif
ARCH ?= arm

TARGET = libSDL-1.2.so.0
OBJS += standalone.o osdl_input.o osdl_video.o config.o \
	linux/fbdev.o linux/oshide.o
ifeq ($(ARCH),arm)
LDFLAGS += -lts
OBJS += arm_utils.o
else
CFLAGS += -fPIC
endif
CFLAGS += -DSTANDALONE

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -shared -o $@ $^ $(LDFLAGS)

clean:
	$(RM) $(TARGET) $(OBJS)

*.o: omapsdl.h
