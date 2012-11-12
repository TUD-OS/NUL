The program tests the basic functionality of sending/receiving data to/by the 
IP stack. The test opens a TCP port and a UDP port and waits for incoming data.
Whenever data is received the number of received bytes are printed.
Additionally the test opens a connection to itself via the TCP port and tries
to send data.

The wv script can be used to start the test in Qemu.

You may send data to the test issuing following commands on your host machine: 

- nc -q 1 127.0.0.1 7777 <somefile                                              
- nc -u -q 1 127.0.0.1 5555 <somefile
