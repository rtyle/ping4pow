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
	
	rm -r .esphome/build/<project>	# rebuild everything
	esphome compile <project>.yaml

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

	esphome upload <project>.yaml

encrypt

	CONFIG_SECURE_FLASH_ENC_ENABLED: "y"
	CONFIG_SECURE_FLASH_ENCRYPTION_AES256: "y"

	CONFIG_NVS_ENCRYPTION: "n"
	CONFIG_NVS_SEC_KEY_PROTECT_USING_FLASH_ENC: "n"

	rm -r .esphome/build/<project>	# rebuild everything
	esphome run <project>.yaml

		# first time...
		# encryption keys will be randomly created and burned in efuses
		# the flash will be encrypted
		# subsequently, this will only work for OTA - not USB
		# esphome does not use the write-flash esptool with --encrypt as it needs to
		# this will result in a never-ending boot loop complaint like

			[11:31:08.073]invalid header: 0x487325b7

	bin=.esphome/build/ping4pow/.pioenvs/ping4pow

	# factory reset (necessary when esphome upload over usb corrupts things)
	python -m esptool --chip esp32s3 --port /dev/ttyACM0 write-flash --encrypt 0x0 $bin/firmware.factory.bin

	# firmware update (
	python -m esptool --chip esp32s3 --port /dev/ttyACM0 write-flash --encrypt 0x10000 $bin/firmware.bin 0xe10000 $bin/ota_data_initial.bin
