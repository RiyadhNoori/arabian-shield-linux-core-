# Arabian Shield — Fonts

This directory contains the font download script. Actual font files are **not**
included in this repository, as they are licensed separately.

## Recommended Fonts

| Font | Style | License | Coverage |
|------|-------|---------|----------|
| **Noto Sans Arabic** | Sans-serif, UI | OFL 1.1 | Full Arabic block + Extended |
| **Noto Naskh Arabic** | Serif (Naskh) | OFL 1.1 | Full Arabic block + Tashkeel |
| **Amiri** | Serif (classical) | OFL 1.1 | Full Arabic + Quranic marks |
| **Scheherazade New** | Serif (liturgical) | OFL 1.1 | Full Arabic + rare characters |
| **Cairo** | Geometric sans | OFL 1.1 | Arabic + Latin |
| **Vazirmatn** | Sans-serif | OFL 1.1 | Persian/Farsi optimized |
| **Noto Nastaliq Urdu** | Nastaliq | OFL 1.1 | Urdu |

## Download

Run the download script to fetch all recommended fonts:

```bash
bash fonts/download_fonts.sh
```

This will download fonts to `/usr/local/share/fonts/arabian-shield/` (system-wide)
or `~/.local/share/fonts/arabian-shield/` (user install) and refresh the font cache.
