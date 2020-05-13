CC = xtensa-lx106-elf-gcc
CFLAGS = -I. -mlongcalls
LDLIBS = -nostdlib -Wl,--start-group -lmain -lnet80211 -lwpa -llwip -lpp -lphy -lc -Wl,--end-group -lgcc
LDFLAGS = -Teagle.app.v6.ld

wifi_scan-0x00000.bin: wifi_scan
	esptool.py elf2image $^

wifi_scan.o: wifi_scan.c

wifi_scan: wifi_scan.o driver/uart.o
	$(CC) $(LDFLAGS) $^ -o $@ $(LDLIBS)

flash: wifi_scan-0x00000.bin
	esptool.py write_flash 0 wifi_scan-0x00000.bin 0x10000 wifi_scan-0x10000.bin

clean:
	rm -f wifi_scan wifi_scan.o wifi_scan-0x00000.bin wifi_scan-0x10000.bin
