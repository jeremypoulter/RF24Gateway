RF24Gateway: main.cpp
	g++ -Wall -Ofast -mfpu=vfp -mfloat-abi=hard -march=armv6zk -mtune=arm1176jzf-s -I./RF24/RPi/RF24/ -lrf24-bcm -lcurl main.cpp -o RF24Gateway