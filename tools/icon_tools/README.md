# Icon converter usage

This tool converts PNG icons into C arrays compatible with `OLED_DrawBitmap()`.

## 1) Install dependency

```bash
pip install pillow
```

## 2) Convert one icon

Run from project root:

```bash
python3 tools/icon_tools/png_to_oled_c.py \
  assets/icons/menu/heart.png \
  --width 16 --height 16 \
  --name MICON_HEART \
  --out-h assets/icons/generated/micon_heart.h \
  --out-c assets/icons/generated/micon_heart.c
```

If your icon appears inverted on OLED, add `--invert`.

## 3) Integrate into firmware

Already auto-wired. The project `Makefile` compiles generated icon sources from `assets/icons/generated/` and `oled.c` includes those generated headers.

Current menu expects 6 symbols in this order:

1. `MICON_HEART`
2. `MICON_SPO2`
3. `MICON_WORKOUT`
4. `MICON_STOPWATCH`
5. `MICON_STATS`
6. `MICON_SETTINGS`
