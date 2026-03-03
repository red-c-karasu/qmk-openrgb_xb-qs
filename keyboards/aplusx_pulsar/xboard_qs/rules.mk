# MCU and bootloader (from keyboard.json processor/bootloader fields)
MCU = STM32F072
BOOTLOADER = stm32-dfu

# Build options
LTO_ENABLE = yes
BOOTMAGIC_ENABLE = yes
MOUSEKEY_ENABLE = yes
EXTRAKEY_ENABLE = yes
NKRO_ENABLE = yes
ENCODER_ENABLE = yes
RGB_MATRIX_ENABLE = yes

# Storage (from keyboard.json eeprom section)
EEPROM_DRIVER = wear_leveling
WEAR_LEVELING_DRIVER = embedded_flash

# WS2812 via SPI (from keyboard.json ws2812.driver)
WS2812_DRIVER = spi

# Dynamic macro support (used in via keymap)
DYNAMIC_MACRO_ENABLE = yes
