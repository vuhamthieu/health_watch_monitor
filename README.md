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
<img width="1536" height="2048" alt="670879143_2217160605767582_4890174502395803466_n" src="https://github.com/user-attachments/assets/9b2d23bf-9b67-4f18-a0f4-bbc889966cb2" />
<img width="1536" height="2048" alt="654427559_1268526301572555_1430042917183493648_n" src="https://github.com/user-attachments/assets/fb6a6a42-96d4-4739-ae43-4a50c50e5ef6" />
<img width="1536" height="2048" alt="672155729_829751799606275_236258068461786958_n" src="https://github.com/user-attachments/assets/eda38683-59ce-44cd-aab7-fc1202705733" />

## Notes

- Linker script: `STM32F103C8TX_FLASH.ld`
- Generated/compiled output: `build/`
