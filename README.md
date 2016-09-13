# RF24Gateway

## Instilation

If using the EmonPi SD card image ensure you are in RW mode;
```
rpi-wr
```

Download the prerequisites

```
sudo apt-get update
sudo apt-get install libcurl4-openssl-dev
```

Install and build the RF24 library

```
git clone https://github.com/TMRh20/RF24.git
cd RF24
make
sudo make install
```

Enable SPI and I2C in rasp-config

```
sudo rasp-config
```

Reboot

Install RF24Gateway

```
git clone https://github.com/jeremypoulter/RF24Gateway.git
cd RF24Gateway
```
