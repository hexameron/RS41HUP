# Vaisala RS41 Expansion Connector Pinout
Based on work by DF8OE


Viewed into port from outside
```
----------|     |----------
|  9    7    5    3    1  |
|                         |
|  10   8    6    4    2  |
---------------------------
```

Suitable Plug: Molex 87568-1073  (element14 order code 1365756)  
Adafruit sell 2.54mm to 2mm patch leads (available from other resellers)

* 1 - GND
* 2 - I2C_SDA (PB11)
* 3 - I2C_SCL (PB10)
* 4 - +VDD_MCU
* 5 - 3.6V from boost converter
* 6 - +V_Battery
* 7 - RST
* 8 - SWCLK (PA14)
* 9 - SWDIO (PA13)
* 10 - GND



## STLink-v2 Header to Vaisala Cable
```
RS41		STLink		UART
------------------------------------
1		(GND)		GND
2		N/C		RX
3		N/C		TX
4		POWER	1
5		N/C		(3.6v)
6		N/C
7		RESET	15
8		CLOCK	9
9		DATA	7
10		GND	4
```
All other pins on STLink header not connected.  
When using the Adafruit patch leads it is more convenient to use
pin 10 as GND, since the 2mm ends are coupled as pairs. It is safe
to supply 3.3v on the programming header, but this allows power to
pass back through the CPU Regulator, and may not be the best idea.
