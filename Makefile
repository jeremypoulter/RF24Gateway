C_FLAGS = -Ofast -mfpu=vfp -mfloat-abi=hard -march=armv6zk -mtune=arm1176jzf-s  
LIBS = -lrf24-bcm 
#C_FLAGS = -ggdb -DTEST
#LIBS = 

C_FLAGS += -Wall -std=c++11 $(shell pkg-config jsoncpp --cflags)
LIBS += -lcurl -lmosquitto $(shell pkg-config jsoncpp --libs)

TARGET = RF24Gateway

.PHONY: all install clean

all: $(TARGET)

install: $(TARGET) /etc/$(TARGET)
	cp $(TARGET) /usr/local/bin/$(TARGET)
	cp service /etc/init.d/$(TARGET)

clean:
	rm $(TARGET)

$(TARGET): main.cpp
	g++ $(C_FLAGS) $? -o $@ $(LIBS)

/etc/$(TARGET): Config
	cp Config /etc/$(TARGET)

