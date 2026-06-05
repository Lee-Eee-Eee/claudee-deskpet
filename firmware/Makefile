PROJECT  ?= app

PREFIX   := arm-none-eabi-
CC       := $(PREFIX)gcc
AS       := $(PREFIX)gcc -x assembler-with-cpp
LD       := $(PREFIX)gcc
OBJCOPY  := $(PREFIX)objcopy
SIZE     := $(PREFIX)size

JLINK    ?= "C:/Program Files/SEGGER/JLink_V934b/JLink.exe"

MCU      := -mcpu=cortex-m0plus -mthumb
OPT      := -Os
DEBUG    := -g3 -gdwarf-2
WARN     := -Wall -Wextra -Wno-unused-parameter -Wno-unused-variable -Wno-unused-but-set-variable

BUILD    := build

C_SRCS   := $(wildcard Sources/*.c) \
            $(wildcard Project_Settings/Startup_Code/*.c)
S_SRCS   := $(wildcard Project_Settings/Startup_Code/*.S)

INCS     := -IProject_Headers \
            -IProject_Settings/Startup_Code \
            -ISources

DEFS     := -DCPU_MKL25Z128VLK4

LDSCRIPT := Project_Settings/Linker_Files/MKL25Z128_flash.ld

CFLAGS   := $(MCU) $(OPT) $(DEBUG) $(WARN) $(INCS) $(DEFS) \
            -ffunction-sections -fdata-sections \
            -fno-common -fmessage-length=0 \
            -std=gnu99

LDFLAGS  := $(MCU) -T$(LDSCRIPT) \
            -Wl,--gc-sections \
            -Wl,-Map=$(BUILD)/$(PROJECT).map \
            -nostartfiles \
            --specs=nano.specs \
            --specs=nosys.specs

OBJS     := $(patsubst %.c,$(BUILD)/%.o,$(C_SRCS)) \
            $(patsubst %.S,$(BUILD)/%.o,$(S_SRCS))
DEPS     := $(OBJS:.o=.d)

.PHONY: all clean flash erase size help
.DEFAULT_GOAL := all

all: $(BUILD)/$(PROJECT).elf $(BUILD)/$(PROJECT).bin $(BUILD)/$(PROJECT).hex size

$(BUILD)/%.o: %.c
	@if not exist $(subst /,\,$(dir $@)) mkdir $(subst /,\,$(dir $@))
	@echo "CC  $<"
	@$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/%.o: %.S
	@if not exist $(subst /,\,$(dir $@)) mkdir $(subst /,\,$(dir $@))
	@echo "AS  $<"
	@$(AS) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/$(PROJECT).elf: $(OBJS)
	@echo "LD  $@"
	@$(LD) $(OBJS) $(LDFLAGS) -o $@

$(BUILD)/%.bin: $(BUILD)/%.elf
	@$(OBJCOPY) -O binary $< $@

$(BUILD)/%.hex: $(BUILD)/%.elf
	@$(OBJCOPY) -O ihex $< $@

size: $(BUILD)/$(PROJECT).elf
	@echo ""
	@$(SIZE) $<

clean:
	@if exist $(BUILD) rmdir /S /Q $(subst /,\,$(BUILD))
	@echo "Cleaned $(BUILD)/"
flash: $(BUILD)/$(PROJECT).bin
	@echo "Flashing via J-Link..."
	@echo device MKL25Z128xxx4> $(BUILD)/flash.jlink
	@echo if SWD>> $(BUILD)/flash.jlink
	@echo speed 4000>> $(BUILD)/flash.jlink
	@echo loadbin $(BUILD)/$(PROJECT).bin 0>> $(BUILD)/flash.jlink
	@echo r>> $(BUILD)/flash.jlink
	@echo g>> $(BUILD)/flash.jlink
	@echo q>> $(BUILD)/flash.jlink
	@$(JLINK) -CommanderScript $(BUILD)/flash.jlink

erase:
	@if not exist $(BUILD) mkdir $(BUILD)
	@echo device MKL25Z128xxx4> $(BUILD)/erase.jlink
	@echo if SWD>> $(BUILD)/erase.jlink
	@echo speed 4000>> $(BUILD)/erase.jlink
	@echo erase>> $(BUILD)/erase.jlink
	@echo q>> $(BUILD)/erase.jlink
	@$(JLINK) -CommanderScript $(BUILD)/erase.jlink
help:
	@echo "Targets:"
	@echo "  make           - build $(PROJECT).elf / .bin / .hex"
	@echo "  make clean     - remove $(BUILD)/"
	@echo "  make flash     - flash via J-Link (CLI; usually use VSCode F5 instead)"
	@echo "  make erase     - mass-erase flash"
	@echo ""
	@echo "Set PROJECT=name to change output name (default: app)"

-include $(DEPS)
