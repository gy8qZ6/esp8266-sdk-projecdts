CC = xtensa-lx106-elf-gcc
CFLAGS = -I. -mlongcalls -std=c99
LDLIBS = -nostdlib -Wl,--start-group -lmain -lnet80211 -lwpa -llwip -lpp -lphy -lc -Wl,--end-group -lgcc -ldriver
LDFLAGS = -Teagle.app.v6.ld

main-0x00000.bin: main
	esptool.py elf2image $^

main.o: main.c

main: main.o ssd1306.o driver/i2c_master.o #driver/uart.o
	$(CC) $(LDFLAGS) $^ -o $@ $(LDLIBS)

flash: main-0x00000.bin
	esptool.py write_flash 0 main-0x00000.bin 0x10000 main-0x10000.bin

clean:
	rm -f main main.o ssd1306.o driver/i2c_master.o driver/uart.o main-0x00000.bin main-0x10000.bin
