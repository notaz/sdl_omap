CC = $(CROSS_COMPILE)gcc
AS = $(CROSS_COMPILE)as
CFLAGS += -Wall -ggdb -O2
#ASFLAGS = -mcpu=cortex-a8

TARGET = libSDL-1.2.so.0
OBJS += main.o arm_utils.o linux/fbdev.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -shared -o $@ $^

clean:
	$(RM) $(TARGET) $(OBJS)
