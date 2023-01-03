# ESP32-S2 USB DMX-512 Controller

Designed for non-admin use from Linux or Windows.

Maps as a MAGFest Swadge.

D+ is on GPIO17 and D- is on GPIO18.

See demo on how to send DMX-512 frames in the testapp folder.

Note that this has a unique "serial" and that should be what is `hid_open`ed.

## Build

Build against IDF v4.4.1

## Flash

(From build folder)
```
esptool.py esp32s2 -p /dev/ttyUSB0 -b 460800 --before=default_reset --after=hard_reset write_flash --flash_mode dio --flash_freq 80m --flash_size 2MB 0x1000 bootloader/bootloader.bin 0x10000 esp32_s2_dmx.bin 0x8000 partition_table/partition-table.bin
```


