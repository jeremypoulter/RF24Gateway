TARGET = RF24Gateway

.PHONY: all install clean

all: $(TARGET)

install: $(TARGET) /etc/$(TARGET)
	cp $(TARGET) /usr/local/bin/$(TARGET)
	cp service /etc/init.d/$(TARGET)

clean:
	rm $(TARGET)

$(TARGET): main.cpp
	g++ -Wall -Ofast -mfpu=vfp -mfloat-abi=hard -march=armv6zk -mtune=arm1176jzf-s -lrf24-bcm -lcurl $? -o $@

/etc/$(TARGET): Config
	cp Config /etc/$(TARGET)

