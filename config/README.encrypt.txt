https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/security/flash-encryption.html

> Enabling flash encryption will increase the size of bootloader,
> which might require updating partition table offset. See Bootloader Size.

	bootloader size is constrained by
	the location (default 0x8000) of the partition table.
	the partition table is 0xc00 bytes but must be aligned on 0x1000 byte sectors.
	esphome/esp-idf tools expect the app to be loaded at 0x10000 in flash.
	we can easily almost double the bootloader size to 0xf000
	but we must also redefine (from the defaults) the partitions.
	while we are at it, use all the 16MB from an m5stack cores3.

		esp32:
		  board: m5stack-cores3
		  framework:
		    type: esp-idf
		    sdkconfig_options:
		      CONFIG_PARTITION_TABLE_OFFSET: "0xf000"
		  partitions: partitions.csv
		  flash_size: 16MB

	partitions.csv

		app0,		app,	ota_0,	0x10000,	0x700000,
		app1,		app,	ota_1,	,	0x700000,
		otadata,	data,	ota,	,	0x2000,
		phy_init,	data,	phy,	,	0x1000,
		nvs,		data,	nvs,	,	0x6D000,

	app0 & app1 enlarged to 7M and ordered first.
	otadata, phy_init and nvs keep esphome default size but ordered last.
	
	rm -r .esphome/build/ping4pow	# rebuild everything
	esphome compile ping4pow.yaml

	~/.platformio/packages/framework-espidf/components/partition_table/gen_esp32part.py .esphome/build/ping4pow/.pioenvs/ping4pow/partitions.bin

		Parsing binary partition input...
		Verifying table...
		# ESP-IDF Partition Table
		# Name, Type, SubType, Offset, Size, Flags
		app0,app,ota_0,0x10000,7M,
		app1,app,ota_1,0x710000,7M,
		otadata,data,ota,0xe10000,8K,
		phy_init,data,phy,0xe12000,4K,
		nvs,data,nvs,0xe13000,436K,

	esphome upload ping4pow.yaml

encrypt

	CONFIG_SECURE_FLASH_ENC_ENABLED: "y"
	CONFIG_SECURE_FLASH_ENCRYPTION_AES256: "y"

	CONFIG_NVS_ENCRYPTION: "n"
	CONFIG_NVS_SEC_KEY_PROTECT_USING_FLASH_ENC: "n"

	rm -r .esphome/build/ping4pow	# rebuild everything

	esphome run ping4pow.yaml --device=/dev/ttyACM0

esptool v5.1.0
Connected to ESP32-S3 on /dev/ttyACM0:
Chip type:          ESP32-S3 (QFN56) (revision v0.2)
Features:           Wi-Fi, BT 5 (LE), Dual Core + LP Core, 240MHz
Crystal frequency:  40MHz
USB mode:           USB-Serial/JTAG
MAC:                c0:4e:30:??:??:??

Stub flasher running.
Changing baud rate to 460800...
Changed.

Configuring flash size...
Auto-detected flash size: 16MB
Flash will be erased from 0x00010000 to 0x001a8fff...							# firmware
Flash will be erased from 0x00000000 to 0x00007fff...							# bootloader
Flash will be erased from 0x0000f000 to 0x0000ffff...							# partitions
Flash will be erased from 0x00e10000 to 0x00e11fff...							# nvs

Wrote 1671184 bytes (952052 compressed) at 0x00010000 in 13.8 seconds (970.6 kbit/s).			# firmware.bin
Hash of data verified.
SHA digest in image updated.
Wrote 31344 bytes (19345 compressed) at 0x00000000 in 0.6 seconds (425.0 kbit/s).			# bootloader.bin
Hash of data verified.
Wrote 3072 bytes (130 compressed) at 0x0000f000 in 0.0 seconds (516.0 kbit/s).				# partitions.bin
Hash of data verified.
Wrote 8192 bytes (31 compressed) at 0x00e10000 in 0.1 seconds (708.6 kbit/s).				# nvs
Hash of data verified.

Hard resetting via RTS pin...
INFO Successfully uploaded program.
INFO Starting log output from /dev/ttyACM0 with baud rate 115200

[08:03:34.811]I (395) esp_image: segment 3: paddr=00003440 vaddr=403cb700 size=0460ch ( 17932) 
[08:03:35.267]I (850) flash_encrypt: bootloader encrypted successfully					# bootloader
[08:03:35.313]I (896) flash_encrypt: partition table encrypted and loaded successfully			# partitions
[08:03:35.314]I (897) esp_image: segment 0: paddr=00010020 vaddr=3c100020 size=90390h (590736) map
[08:03:35.419]I (1001) esp_image: segment 1: paddr=000a03b8 vaddr=3fc95a00 size=030e4h ( 12516) 
[08:03:35.421]I (1004) esp_image: segment 2: paddr=000a34a4 vaddr=40374000 size=0cb74h ( 52084) 
[08:03:35.431]I (1013) esp_image: segment 3: paddr=000b0020 vaddr=42000020 size=f3158h (995672) map
[08:03:35.606]I (1189) esp_image: segment 4: paddr=001a3180 vaddr=40380b74 size=04e48h ( 20040) 
[08:03:35.610]I (1193) esp_image: segment 5: paddr=001a7fd0 vaddr=600fe000 size=0001ch (    28) 
[08:03:35.611]I (1194) flash_encrypt: Encrypting partition 0 at offset 0x10000 (length 0x198010)...	# firmware
[08:03:58.073]I (23657) flash_encrypt: Done encrypting
[08:03:58.074]E (23657) esp_image: image at 0x710000 has invalid magic byte (nothing flashed here?)	# nothing
[08:03:58.075]I (23657) flash_encrypt: Encrypting partition 2 at offset 0xe10000 (length 0x2000)...	# ota_data
[08:03:58.167]I (23750) flash_encrypt: Done encrypting
[08:03:58.167]I (23750) efuse: BURN BLOCK0
[08:03:58.169]I (23752) efuse: BURN BLOCK0 - OK (all write block bits are set)
[08:03:58.169]I (23753) flash_encrypt: Flash encryption completed
[08:03:58.170]I (23753) boot: Resetting with flash encryption enabled...

[08:03:58.171]ESP-ROM:esp32s3-20210327
[08:03:58.172]Build:Mar 27 2021
[08:03:58.173]rst:0x3 (RTC_SW_SYS_RST),boot:0x2b (SPI_FAST_FLASH_BOOT)
[08:03:58.173]Saved PC:0x403ccf35
[08:03:58.189]SPIWP:0xee
[08:03:58.190]mode:DIO, clock div:1
[08:03:58.190]load:0x3fce2980,len:0x2784
[08:03:58.190]load:0x403c8700,len:0x4
[08:03:58.191]load:0x403c8704,len:0xc80
[08:03:58.191]load:0x403cb700,len:0x460c
[08:03:58.202]entry 0x403c890c

[08:03:58.205]I (33) boot: ESP-IDF 5.4.2 2nd stage bootloader
[08:03:58.205]I (33) boot: compile time Dec 23 2025 08:00:08
[08:03:58.206]I (34) boot: Multicore bootloader
[08:03:58.207]I (34) boot: chip revision: v0.2
[08:03:58.207]I (34) boot: efuse block revision: v1.3
[08:03:58.208]I (34) boot.esp32s3: Boot SPI Speed : 80MHz
[08:03:58.209]I (34) boot.esp32s3: SPI Mode       : DIO
[08:03:58.210]I (35) boot.esp32s3: SPI Flash Size : 16MB
[08:03:58.211]I (35) boot: Enabling RNG early entropy source...
[08:03:58.211]I (35) boot: Partition Table:
[08:03:58.212]I (35) boot: ## Label            Usage          Type ST Offset   Length
[08:03:58.212]I (36) boot:  0 app0             OTA app          00 10 00010000 00700000
[08:03:58.213]I (36) boot:  1 app1             OTA app          00 11 00710000 00700000
[08:03:58.214]I (36) boot:  2 otadata          OTA data         01 00 00e10000 00002000
[08:03:58.214]I (37) boot:  3 phy_init         RF data          01 01 00e12000 00001000
[08:03:58.215]I (37) boot:  4 nvs              WiFi data        01 02 00e13000 0006d000
[08:03:58.215]I (38) boot: End of partition table
[08:03:58.216]I (38) esp_image: segment 0: paddr=00010020 vaddr=3c100020 size=90390h (590736) map
[08:03:58.328]I (156) esp_image: segment 1: paddr=000a03b8 vaddr=3fc95a00 size=030e4h ( 12516) load
[08:03:58.331]I (159) esp_image: segment 2: paddr=000a34a4 vaddr=40374000 size=0cb74h ( 52084) load
[08:03:58.344]I (172) esp_image: segment 3: paddr=000b0020 vaddr=42000020 size=f3158h (995672) map
[08:03:58.543]I (371) esp_image: segment 4: paddr=001a3180 vaddr=40380b74 size=04e48h ( 20040) load
[08:03:58.548]I (376) esp_image: segment 5: paddr=001a7fd0 vaddr=600fe000 size=0001ch (    28) load
[08:03:58.555]I (383) boot: Loaded app from partition at offset 0x10000
[08:03:58.555]I (384) boot: Checking flash encryption...
[08:03:58.555]I (384) flash_encrypt: flash encryption is enabled (1 plaintext flashes left)
[08:03:58.555]I (384) boot: Disabling RNG early entropy source...

		# ^ first time...
		# encryption keys will be randomly created and burned in efuses
		# the flash will be encrypted

		# subsequently, this will only work for OTA - not USB, because
		# esphome does not use the write-flash esptool with --encrypt as it needs to
		# this will result in a never-ending boot loop complaint
		# from the first stage bootbootloader like

			[11:31:08.073]invalid header: 0x487325b7

		# use OTA updates only


	# factory reset (necessary when esphome upload over USB corrupts things)
	bin=.esphome/build/ping4pow/.pioenvs/ping4pow
	python -m esptool --chip esp32s3 --port /dev/ttyACM0 write-flash --encrypt 0x0 $bin/firmware.factory.bin

	# firmware.bin & ota_data_initial.bin update only
	python -m esptool --chip esp32s3 --port /dev/ttyACM0 write-flash --encrypt 0x10000 $bin/firmware.bin 0xe10000 $bin/ota_data_initial.bin
