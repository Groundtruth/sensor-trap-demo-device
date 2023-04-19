# Sensor Trap Demo Device

This repo implements a simple LoRaWAN device to act as a trap sensor.
It is not fit to deploy, but demonstrates the principle and behaviour of such a device.
It provides code to be loaded on a [LILYGO TTGO T-Beam](http://www.lilygo.cn/claprod_view.aspx?TypeId=62&Id=1281&FId=t28:62:28).

## Principle

The purpose of a sensor trap is to remotely monitor a pest trap.
In general, a false positive (report as sprung, while the trap is physically still set) is a nuisance, but a false negative (report as set, while the trap is physically still sprung) is not at all acceptable in the case of live capture traps.
This principle must be factored at every stage from the physical sensor (which should fail towards reporting sprung), to the device behaviour (e.g. sending periodic heartbeats to confirm set status), to the LoRaWAN application (see [sensor-trap-application](https://github.com/Groundtruth/sensor-trap-reference-application)) and end-user application.

## Hardware

The software supports the OLED display provided with the V1.1 T-Beam, connected on pins 21 and 22.
The software, for demonstration purposes, senses trap state by measuring between pin 13 and ground. A closed circuit corresponds to the "set" state and an open circuit corresponds to the "sprung" state.

## Behaviour

### State

Two pieces of state persist across deep sleep periods and contribute to the device's behaviour:
* The previous state (either set or sprung)
* The time of the previously sent message

### Wake-up Sources

The device spends most of its time sleeping to conserve power. Three conditions cause the device to wake up:
* Reset, when the device first boots or the reset button is pressed
* Button press, when then builtin (middle) IO38 button is pressed
* Timer, every `SLEEP_INTERVAL` seconds

### Logic

Every time the device wakes:

1. If the device just woke from a reset, configure the power supply
2. If the device just woke from a button press, start a GUI task:
   1. Turn on and configure the OLED display
   2. Show a message for 5 seconds
   3. Turn off the OLED display
   4. Signal the main task and finish
3. The sensor is read (either "set" or "sprung")
4. The new sensor value is compared with the old sensor value, to determine a new event (either "set", "sprung", or "none")
5. If the event is "none", but the next wakeup time will be past the heartbeat interval, set the event to "heartbeat"
6. Store the new sensor value over the old sensor value
7. If the event is anything but "none", turn on the LoRa chip and send a LoRaWAN message containing the state, event, and battery voltage.
8. Turn off the LoRa chip, wait for the GUI task to complete, configure wake conditions, and go to deep sleep

## Configuration

Static configuration is driven by a couple of macros found in `main/sensor-trap-demo.c`.

* `SLEEP_INTERVAL` (seconds) determines how often the device wakes to check sensors and send heartbeats
* `HEARTBEAT_INTERVAL` (seconds) determines how often the device should send a heartbeat message, regardless of status changes

Device provisioning information is configured by AT commands once a device is programmed.
The LoRaWAN OTAA parameters (DevEUI, AppEUI, and AppKey) are provided with a command in the format:

```
AT+PROV=DDDDDDDDDDDDDD-AAAAAAAAAAAAAAAA-KKKKKKKKKKKKKKKKKKKKKKKK
```

Where `DD..DD` is the DevEUI, `AA..AA` is the AppEUI (sometimes called JoinEUI), and `KK..KK` is the AppKey.
These parameters are set/generated when provisioning a device in The Things Network.

See the relevant page in the [ttn-esp32 documentation](https://github.com/manuelbl/ttn-esp32/wiki/AT-Commands) for more details.

## Building and flashing

The project is built with the [Espressif IoT Development Framework](https://docs.espressif.com/projects/esp-idf/en/v5.0.1/esp32/get-started/index.html) (ESP-IDF), version 5.0.1.
The simplest way to build and load this project onto a T-Beam is to use their Docker image. 

To build the project, in the project directory, run
```
docker run -it --rm -v $PWD:/project -w /project espressif/idf:v5.0.1 idf.py build
```

To build and flash the project, in the project directory, with the T-Beam connected by USB as `/dev/ttyACM0`, run
```
docker run -it --rm --device /dev/ttyACM0 -v $PWD:/project -w /project espressif/idf:v5.0.1 sh -c idf.py flash
```

Other ESP-IDF functions (e.g. `menuconfig`, `monitor`) can be performed similarly.
