
**I2S Bluetooth Transmitter**

Such boards can be connected directly to the I2S output:

![image](https://github.com/user-attachments/assets/9e2d8066-f41e-41eb-9db5-e7b7a6e554e8)

If you still have an old ESP32 in your box, you can use it to simulate this board. PSRAM is not required.

The BT transmitter is the slave and is connected in this way, the DAC serves as an analogue output, but is not necessary.

![image](https://github.com/user-attachments/assets/ac17cfa3-e473-4750-94ce-ee218827b3c3)


The ESP32-A2DP library is used by P. Schatzmann, https://github.com/pschatzmann/ESP32-A2DP.git

As the I2S output of the audioI2S library always outputs 48KHz, it is scaled internally to 44.1KHz for compatibility.
This is necessary because the ESP32 BT library expects this sample rate. This means that old BT devices can also be used.
It doesn't matter what sample rate the audio source has.

![image](https://github.com/user-attachments/assets/0009dd9d-96b2-48b7-a6cc-bfc45dbc94d0)

Test circuit: the audioI2S library is running on the left, the BT transmitter on the right


