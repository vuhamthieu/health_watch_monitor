# Health Watch Monitor (STM32)

Firmware for an STM32-based health watch / wearable monitor.

## What’s inside

- STM32 project generated from `health_watch_monitor.ioc` (CubeMX)
- FreeRTOS-based application
- Drivers/modules for typical wearable sensors (see `Core/` for details)

## Build / flash

Prereqs: `arm-none-eabi-gcc`, `make`, and an ST-Link setup.

```bash
make -j$(nproc)     # build
make clean          # clean
make flash          # flash via ST-Link (per Makefile)
make size           # size report
```

Optional OpenOCD server:

```bash
openocd -f interface/stlink.cfg -f target/stm32f1x.cfg
```

## Images


## Notes

- Linker script: `STM32F103C8TX_FLASH.ld`
- Generated/compiled output: `build/`
