# Hardware references

Reference links checked during the documentation review on 2026-11-04. Third-party material retains its original copyright and license. This repository links to documentation rather than redistributing vendor bundles.

| Reference | Source | Applicability |
|---|---|---|
| ESP32-S3-DevKitC-1 | [Espressif documentation](https://documentation.espressif.com/api/resource/path/docs/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-devkitc-1/index.html) | Board used in the Wokwi simulation. Compare the physical vendor board before using official pinouts or schematics. |
| ESP32-S3 technical documents | [Espressif document library](https://www.espressif.com/en/support/documents/technical-documents) | Select the exact chip/module revision and N16R8 configuration. |
| ILI9341 controller and 2.8-inch SPI module | [LCDWIKI module page](https://www.lcdwiki.com/2.8inch_SPI_Module_ILI9341_SKU%3AMSP2807) | Links to the controller datasheet and module documentation. Access to a PDF does not grant redistribution rights. |
| QD-TFT2803 panel specification v1.1 | [Supplier document index](https://www.lcdwiki.com/res/MSP2807/) | Match the panel marking before applying its mechanical/electrical specifications. |
| MSP2806/MSP2807 schematic and manuals | [LCDWIKI documents](https://www.lcdwiki.com/2.8inch_SPI_Module_ILI9341_SKU%3AMSP2807) | LCDWIKI labels MSP2806 as non-touch and MSP2807 as touch; verify the actual module and revision. |
| Purchased N16R8 board | [Vendor homepage](https://www.zave.xin/) | Original package contained ESP32-S3-SCH.pdf, ESP32-S3-inch.pdf and ESP32-S3-Metric.pdf. Exact product URL and board equivalence are unverified. These are board drawings, not a chip datasheet. |
| Adafruit GFX | [Upstream library](https://github.com/adafruit/Adafruit-GFX-Library) | Install through the Arduino Library Manager; follow upstream licensing. |
| Adafruit ILI9341 | [Upstream library](https://github.com/adafruit/Adafruit_ILI9341) | Display driver used by this project. |
| Wokwi | [Documentation](https://docs.wokwi.com/) | Simulation behavior and supported parts. |

The local ILI9341 V1.09 PDF requires prior written permission for redistribution. The vendor packages were removed from this working tree after a verified local backup; they are not covered by the project's MIT license. Older Git commits still contain the previous files.

See [hardware notes](hardware-notes.md) for project-specific wiring and verification status.
