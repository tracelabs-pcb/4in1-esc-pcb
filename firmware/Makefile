TARGET   = motor1_commutation
BUILDDIR = build

CC      = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
SIZE    = arm-none-eabi-size

CPU_FLAGS = -mcpu=cortex-m4 -mthumb -mfloat-abi=soft

CFLAGS  = $(CPU_FLAGS) -Wall -Wextra -Og -g3 -ffunction-sections -fdata-sections \
          -ICore/Inc
ASFLAGS = $(CPU_FLAGS) -g3
LDFLAGS = $(CPU_FLAGS) -TSTM32F405RGTX_FLASH.ld -nostdlib \
          -Wl,--gc-sections -Wl,-Map=$(BUILDDIR)/$(TARGET).map

SRCS_C  = $(wildcard Core/Src/*.c)
SRCS_S  = Core/Startup/startup_stm32f405xx.s
OBJS    = $(patsubst Core/Src/%.c,$(BUILDDIR)/%.o,$(SRCS_C)) \
          $(BUILDDIR)/startup_stm32f405xx.o

all: $(BUILDDIR)/$(TARGET).elf $(BUILDDIR)/$(TARGET).bin

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(BUILDDIR)/%.o: Core/Src/%.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILDDIR)/startup_stm32f405xx.o: $(SRCS_S) | $(BUILDDIR)
	$(CC) $(ASFLAGS) -c $< -o $@

$(BUILDDIR)/$(TARGET).elf: $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) -lgcc -o $@
	$(SIZE) $@

$(BUILDDIR)/$(TARGET).bin: $(BUILDDIR)/$(TARGET).elf
	$(OBJCOPY) -O binary $< $@

clean:
	rm -rf $(BUILDDIR)

.PHONY: all clean
