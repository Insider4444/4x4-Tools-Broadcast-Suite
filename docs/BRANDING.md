# Shared 4x4-Tools logo

The standard mark is the **sixteen filled tiles in a 4×4 grid** used by [4x4-Tools-DLSS-5](https://github.com/Insider4444/4x4-Tools-DLSS-5/blob/main/assets/banner.svg). It is shared publisher branding, not an effect-specific symbol. The former four open-square glyph is retired.

Canonical vector: [4x4Tools-mark.svg](../resources/branding/4x4Tools-mark.svg).

Reusable wordmarks: [dark-background SVG](../resources/branding/4x4Tools-logo-dark.svg) and [light-background SVG](../resources/branding/4x4Tools-logo-light.svg). The standalone mark has a transparent background. The wordmark reads **4x4-Tools**. Existing application categories, vendor IDs, product paths and copyright owner remain **4x4Tools** for compatibility.

The grid uses 35-unit rounded tiles, 10-unit gaps and a 170-unit square canvas. Teal runs from `#62edcf` to `#318cae`. Two corner tiles use 42% opacity; the adjacent accent tiles use 62%, matching the DLSS-5 banner. Keep the sixteen-tile shape intact and leave clear space around it. Product names such as Broadcast Suite sit beside/below the publisher wordmark, outside the mark.

Run `python scripts/make-installer-art.py` to generate the multi-size Windows icon, installer header/welcome artwork and wordmarks from the canonical SVG. The script also synchronizes the inline SVG mark in the GitHub hero, so it renders without external SVG references. A generated contact sheet under `build` supports visual inspection.

Use these assets for 4x4-Tools projects without introducing alternate publisher symbols. The original 4x4Tools artwork follows the repository's MIT license. Adobe/NVIDIA logos are not part of this brand kit.
