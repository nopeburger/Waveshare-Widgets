# Third-party material

- `firmware/WidgetDeck/src/core/esp_lcd_sh8601.c` and `.h` come from the [Waveshare ESP32-S3-Touch-AMOLED-1.64 v2 example](https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.64-v2/tree/21b952243d306692774f3b5bfeb3eb5cff10f851/Arduino/examples/06_LVGL_Test), with Espressif's Apache-2.0 notice retained. See [the license text](LICENSES/Apache-2.0.txt). Board and touch wiring follow the same example.
- `tests/vendor/cJSON.c` and `.h` are cJSON 1.7.18 under the MIT license retained in those files. The firmware uses cJSON bundled with Arduino-ESP32.
- `assets/airlines/` contains Simple Icons source SVGs under CC0, with brand attributions in [its license file](assets/airlines/LICENSE.md). Generated masks live in `sky_airlines.h`.
- `sky_airports.h` uses a selected regional subset of [OurAirports](https://ourairports.com/data/) public-domain airport coordinates for circling detection near Bay Area airports. It is not a global airport catalog.
- The data feeds belong to [ADSB.lol](https://www.adsb.lol/docs/open-data/api/), [EIA](https://www.eia.gov/opendata/), [USGS](https://earthquake.usgs.gov/fdsnws/event/1/), [NIFC](https://data-nifc.opendata.arcgis.com/), and [NOAA SWPC](https://www.swpc.noaa.gov/). Respect their terms and attribution when redistributing data.

The MIT license at the repository root applies to original project code and documentation. Upstream notices and licenses govern the third-party material above.
