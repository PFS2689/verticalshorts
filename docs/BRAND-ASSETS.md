# Brand Assets & Trademark Notice — Vertical Shorts Plugin

**Date obtained:** 2026-08-04 (updated for polished selector; release metadata refreshed 2026-08-08)  
**Plugin version:** 1.0.10

## Trademark disclaimer

YouTube, Twitch, TikTok, Instagram, and related names and marks are trademarks of their respective owners. Vertical Shorts is an independent OBS Studio plugin and is **not** affiliated with, endorsed by, or sponsored by YouTube, Google, Twitch, Amazon, TikTok, ByteDance, Instagram, Meta, or any other platform listed in the destination selector.

## Platform logo assets (bundled SVG)

Assets live under `data/icons/platforms/` and are compiled into the plugin via `src/vsp-resources.qrc` (`:/vsp/platforms/...`).

| File | Brand | Source | License / redistribution |
|------|-------|--------|--------------------------|
| `youtube.svg` | YouTube | [Simple Icons](https://simpleicons.org/) (`youtube`) with official brand fill `#FF0000` | SVG file CC0-1.0 via Simple Icons; mark is a trademark of Google LLC |
| `twitch.svg` | Twitch | Simple Icons (`twitch`) with official brand fill `#9146FF` | SVG file CC0-1.0 via Simple Icons; mark is a trademark of Twitch Interactive, Inc. |
| `tiktok.svg` / `tiktok-dark.svg` | TikTok | Simple Icons (`tiktok`) black / white fills for light & dark OBS themes | SVG file CC0-1.0 via Simple Icons; mark is a trademark of ByteDance Ltd. |
| `instagram.svg` / `instagram-dark.svg` | Instagram | Simple Icons (`instagram`) brand coral / white fills for theme contrast | SVG file CC0-1.0 via Simple Icons; mark is a trademark of Meta Platforms, Inc. |
| `custom-rtmp.svg` | Custom RTMP | Original globe/network glyph | Original work for this project |
| `placeholder.svg` | Fallback | Original neutral person/frame glyph | Original work — shown if a logo resource is missing |

### Simple Icons attribution

- Project: https://github.com/simple-icons/simple-icons  
- Package version used when obtained: **14.12.3** (jsDelivr npm CDN)  
- License of the SVG files: **CC0 1.0 Universal**  
- Obtained date: **2026-08-04**  
- Brand trademark rights remain with the respective owners; CC0 covers the SVG artwork files as distributed by Simple Icons, not a grant of trademark permission beyond nominative UI identification.

### Asset rules followed

- No scraping of logos from brand websites
- No hotlinking of remote logo files
- No downloading of logos while the plugin is running
- Bundled assets are static SVG (and legacy PNG) resources only
- Logos are shown beside the platform **name in text** (never the sole indicator)
- Transparent backgrounds; crisp SVG rendering via Qt Svg with device-pixel-ratio aware pixmaps
- Theme-aware variants for TikTok / Instagram where a black mark would be invisible on dark OBS themes
- Missing resources fall back to `placeholder.svg`
- Vertical Shorts branding is not placed inside or over platform logos

## Required user setup (platforms)

Users must obtain livestream server URLs and stream keys from each platform’s official dashboard when available. Eligibility for RTMP access varies by account, region, and platform policy.
