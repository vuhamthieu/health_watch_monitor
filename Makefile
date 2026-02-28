######################################
# Health Watch Monitor — Makefile
# MCU  : STM32F103C8T6 @ 64 MHz (HSI PLL ×16)
# RTOS : FreeRTOS v10 (CMSIS-RTOS v1 wrapper)
# Build: arm-none-eabi-gcc
######################################

TARGET   = health_watch_monitor
BUILD_DIR = build

######################################
# C Sources
######################################

# Core application
C_SOURCES += \
Core/Src/main.c \
Core/Src/freertos.c \
Core/Src/stm32f1xx_hal_msp.c \
Core/Src/stm32f1xx_hal_timebase_tim.c \
Core/Src/stm32f1xx_it.c \
Core/Src/syscalls.c \
Core/Src/sysmem.c \
Core/Src/system_stm32f1xx.c

# HAL driver
C_SOURCES += \
Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal.c \
Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_cortex.c \
Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_dma.c \
Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_exti.c \
Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_flash.c \
Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_flash_ex.c \
Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_gpio.c \
Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_gpio_ex.c \
Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_i2c.c \
Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_pwr.c \
Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_rcc.c \
Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_rcc_ex.c \
Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_tim.c \
Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_tim_ex.c \
Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_uart.c

# FreeRTOS kernel
C_SOURCES += \
Middlewares/Third_Party/FreeRTOS/Source/croutine.c \
Middlewares/Third_Party/FreeRTOS/Source/event_groups.c \
Middlewares/Third_Party/FreeRTOS/Source/list.c \
Middlewares/Third_Party/FreeRTOS/Source/queue.c \
Middlewares/Third_Party/FreeRTOS/Source/stream_buffer.c \
Middlewares/Third_Party/FreeRTOS/Source/tasks.c \
Middlewares/Third_Party/FreeRTOS/Source/timers.c \
Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS/cmsis_os.c \
Middlewares/Third_Party/FreeRTOS/Source/portable/MemMang/heap_4.c \
Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM3/port.c

# Application modules (uncomment each as you implement them)
# C_SOURCES += Core/Src/app_config.c
C_SOURCES += Core/Src/sensor_data.c
C_SOURCES += Core/Src/button.c
C_SOURCES += Core/Src/sh1106.c
C_SOURCES += Core/Src/oled.c
C_SOURCES += Core/Src/mpu6050.c
C_SOURCES += Core/Src/max30102.c
C_SOURCES += Core/Src/jdy31.c
C_SOURCES += Core/Src/ui_menu.c
C_SOURCES += Core/Src/power_manager.c
C_SOURCES += Core/Src/step_counter.c
C_SOURCES += Core/Src/heart_rate.c
C_SOURCES += Core/Src/spo2.c

######################################
# Assembly Sources
######################################

ASM_SOURCES = Core/Startup/startup_stm32f103c8tx.s

######################################
# Toolchain
######################################

PREFIX = arm-none-eabi-
CC     = $(PREFIX)gcc
AS     = $(PREFIX)gcc -x assembler-with-cpp
CP     = $(PREFIX)objcopy
SZ     = $(PREFIX)size
HEX    = $(CP) -O ihex
BIN    = $(CP) -O binary -S

######################################
# Compiler Flags
######################################

CPU    = -mcpu=cortex-m3
MCU    = $(CPU) -mthumb

C_DEFS = \
-DUSE_HAL_DRIVER \
-DSTM32F103xB \
-DDEBUG

C_INCLUDES = \
-ICore/Inc \
-IDrivers/STM32F1xx_HAL_Driver/Inc \
-IDrivers/STM32F1xx_HAL_Driver/Inc/Legacy \
-IDrivers/CMSIS/Device/ST/STM32F1xx/Include \
-IDrivers/CMSIS/Include \
-IMiddlewares/Third_Party/FreeRTOS/Source/include \
-IMiddlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS \
-IMiddlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM3

OPT = -Og

CFLAGS  = $(MCU) $(C_DEFS) $(C_INCLUDES) $(OPT)
CFLAGS += -Wall -Wextra -fdata-sections -ffunction-sections
CFLAGS += -g -gdwarf-2
# Dependency generation
CFLAGS += -MMD -MP -MF"$(@:%.o=%.d)"

######################################
# Linker Flags
######################################

LDSCRIPT = STM32F103C8TX_FLASH.ld
LIBS     = -lc -lm -lnosys
LDFLAGS  = $(MCU) -specs=nano.specs -T$(LDSCRIPT) $(LIBS) \
           -Wl,-Map=$(BUILD_DIR)/$(TARGET).map,--cref \
           -Wl,--gc-sections

######################################
# Build Rules
######################################

.PHONY: all clean flash erase size

all: $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/$(TARGET).hex $(BUILD_DIR)/$(TARGET).bin

# Build object files from C sources
OBJECTS  = $(addprefix $(BUILD_DIR)/,$(notdir $(C_SOURCES:.c=.o)))
vpath %.c $(sort $(dir $(C_SOURCES)))

# Build object files from ASM sources
OBJECTS += $(addprefix $(BUILD_DIR)/,$(notdir $(ASM_SOURCES:.s=.o)))
vpath %.s $(sort $(dir $(ASM_SOURCES)))

$(BUILD_DIR)/%.o: %.c Makefile | $(BUILD_DIR)
	$(CC) -c $(CFLAGS) -Wa,-a,-ad,-alms=$(BUILD_DIR)/$(notdir $(<:.c=.lst)) $< -o $@

$(BUILD_DIR)/%.o: %.s Makefile | $(BUILD_DIR)
	$(AS) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS) Makefile
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@
	$(SZ) $@

$(BUILD_DIR)/%.hex: $(BUILD_DIR)/%.elf | $(BUILD_DIR)
	$(HEX) $< $@

$(BUILD_DIR)/%.bin: $(BUILD_DIR)/%.elf | $(BUILD_DIR)
	$(BIN) $< $@

$(BUILD_DIR):
	mkdir -p $@

# Show memory usage
size: $(BUILD_DIR)/$(TARGET).elf
	$(SZ) $<

# Clean build artifacts
clean:
	-rm -fR $(BUILD_DIR)

# Flash via OpenOCD + ST-Link V2
flash: all
	openocd -f interface/stlink.cfg \
	        -f target/stm32f1x.cfg \
	        -c "program $(BUILD_DIR)/$(TARGET).elf verify reset exit"

# Erase entire chip
erase:
	openocd -f interface/stlink.cfg \
	        -f target/stm32f1x.cfg \
	        -c "init" -c "reset halt" \
	        -c "stm32f1x mass_erase 0" -c "exit"

# Include auto-generated dependency files
-include $(wildcard $(BUILD_DIR)/*.d)
