.RECIPEPREFIX := >
SHELL := cmd.exe
.SHELLFLAGS := /C

# -----------------------------------------------------------------------------
# Project configuration
# -----------------------------------------------------------------------------

TARGET := TwoScreens

# Toolchain prefix
CROSS := arm-none-eabi-

# Tools
CC      := $(CROSS)gcc
AS      := $(CROSS)gcc
OBJCOPY := $(CROSS)objcopy
SIZE    := $(CROSS)size

# OpenOCD
OPENOCD         := C:/DevTools/OpenOCD/bin/openocd.exe
OPENOCD_SCRIPTS := C:/DevTools/OpenOCD/openocd/scripts

# Folders
SRC_DIR        := src
BUILD_DIR      := build
VENDOR_INC     := vendor/Include
VENDOR_SRC     := vendor/Source
ARM_CMSIS_INC  := vendor/Core/Include

# 3rd Party
## USB Support
LOCM3_DIR := third_party/libopencm3
LOCM3_LIB := $(LOCM3_DIR)/lib/libopencm3_stm32f1.a

# Sources
C_SOURCES := $(SRC_DIR)/main.c \
             $(SRC_DIR)/display_max7219.c \
             $(SRC_DIR)/display_ws2812.c \
             $(SRC_DIR)/usb_cdc.c \
             $(SRC_DIR)/cli.c \
             $(SRC_DIR)/line.c \
             $(SRC_DIR)/snake.c \
             $(SRC_DIR)/fire.c \
			 $(VENDOR_SRC)/system_stm32f1xx.c

ASM_SOURCES := $(VENDOR_SRC)/startup_stm32f103xb.s

# Objects
C_OBJECTS   := $(BUILD_DIR)/main.o \
               $(BUILD_DIR)/display_max7219.o \
               $(BUILD_DIR)/display_ws2812.o \
               $(BUILD_DIR)/usb_cdc.o \
               $(BUILD_DIR)/cli.o \
               $(BUILD_DIR)/line.o \
               $(BUILD_DIR)/snake.o \
               $(BUILD_DIR)/fire.o \
			   $(BUILD_DIR)/system_stm32f1xx.o

ASM_OBJECTS := $(BUILD_DIR)/startup_stm32f103xb.o
OBJECTS     := $(C_OBJECTS) $(ASM_OBJECTS)

# Outputs
ELF := $(BUILD_DIR)/$(TARGET).elf
BIN := $(BUILD_DIR)/$(TARGET).bin
HEX := $(BUILD_DIR)/$(TARGET).hex
MAP := $(BUILD_DIR)/$(TARGET).map
LDS := STM32F103C8_FLASH.ld

# -----------------------------------------------------------------------------
# MCU flags
# -----------------------------------------------------------------------------

CPU      := -mcpu=cortex-m3
THUMB    := -mthumb
MCUFLAGS := $(CPU) $(THUMB)

DEFS := -DSTM32F103xB -DSTM32F1
INCLUDES := -I$(VENDOR_INC) -I$(ARM_CMSIS_INC) -I$(LOCM3_DIR)/include -IInclude

CFLAGS := $(MCUFLAGS) $(DEFS) $(INCLUDES) -ffunction-sections -fdata-sections -Wall -Wextra -O0 -g3
ASFLAGS := $(MCUFLAGS) $(DEFS) $(INCLUDES) -x assembler-with-cpp -c -g3
LDFLAGS := $(MCUFLAGS) --specs=nano.specs --specs=nosys.specs -Wl,--gc-sections -Wl,-T,$(LDS) -Wl,-Map=$(MAP)

# -----------------------------------------------------------------------------
# Build rules
# -----------------------------------------------------------------------------

all: $(ELF) $(BIN) $(HEX)

$(BUILD_DIR):
> if not exist $(BUILD_DIR) mkdir $(BUILD_DIR)

$(BUILD_DIR)/main.o: $(SRC_DIR)/main.c | $(BUILD_DIR)
> $(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/display_max7219.o: $(SRC_DIR)/display_max7219.c | $(BUILD_DIR)
> $(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/display_ws2812.o: $(SRC_DIR)/display_ws2812.c | $(BUILD_DIR)
> $(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/line.o: $(SRC_DIR)/line.c | $(BUILD_DIR)
> $(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/snake.o: $(SRC_DIR)/snake.c | $(BUILD_DIR)
> $(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/fire.o: $(SRC_DIR)/fire.c | $(BUILD_DIR)
> $(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/usb_cdc.o: $(SRC_DIR)/usb_cdc.c | $(BUILD_DIR)
> $(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/cli.o: $(SRC_DIR)/cli.c | $(BUILD_DIR)
> $(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/system_stm32f1xx.o: $(VENDOR_SRC)/system_stm32f1xx.c | $(BUILD_DIR)
> $(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/startup_stm32f103xb.o: $(VENDOR_SRC)/startup_stm32f103xb.s | $(BUILD_DIR)
> $(AS) $(ASFLAGS) $< -o $@

$(LOCM3_LIB):
> $(MAKE) -C $(LOCM3_DIR) TARGETS="stm32/f1"

$(ELF): $(OBJECTS) $(LOCM3_LIB)
> $(CC) $(OBJECTS) $(LOCM3_LIB) $(LDFLAGS) -o $@

$(BIN): $(ELF)
> $(OBJCOPY) -O binary $< $@

$(HEX): $(ELF)
> $(OBJCOPY) -O ihex $< $@

size: $(ELF)
> $(SIZE) $(ELF)

flash: $(ELF)
> $(OPENOCD) -s $(OPENOCD_SCRIPTS) -f interface/stlink.cfg -f target/stm32f1x.cfg -c "program $(ELF) verify reset exit"

debug-server: $(ELF)
> $(OPENOCD) -s $(OPENOCD_SCRIPTS) -f interface/stlink.cfg -f target/stm32f1x.cfg -c "adapter speed 950"

clean:
> if exist $(BUILD_DIR) rmdir /s /q $(BUILD_DIR)

.PHONY: all size flash debug-server clean