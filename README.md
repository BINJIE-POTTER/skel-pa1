**PA 1 Overview**

In this programming assignment, we implemented a TCP-like protocol using UDP as the base.

For `sender.c`, we have mainly 3 sections:
- `rsend`: to read the contents from the desired file that are not ACKed by the other side from the file and send them to the desired host. 
- `rrecvACK`: to receive ACK which is sent from the host that we are sending packets to and mark the packet indicated by the ACK as sent.
- `getIPv4`: to convert hostname to IPv4 address.

For `receiver.c`, we have only 1 main section:
- `rrecv`: to receive packets sent from the other side and write the content into the desired file. After writing, it sends an ACK which is the index number for the packet back to the sender host to inform that the content of the packet is already written and there is no need to send again.

Important structures are:

```c
typedef struct {
    unsigned int index;
    char data[BUFFER_SIZE];
} Packet;
```
This structure is used to construct a packet to send the index and the actual data to the receiver. The largest data can be sent in a packet is defined as 1024 bytes. The division of the file is that for every 1024 bytes in the to-read file, they are placed inside `data[BUFFER_SIZE]`. For the remainder, they are calculated and sent specially.

```c
typedef struct {
    unsigned int index; // -1 means heading package
    size size_t packetNum; // Total number of packets
} InfoPacket;
```
This structure is used to construct a packet to inform the receiver of the total number of packets that the sender is going to send. This is because the receiver uses this information to terminate and inform the sender to terminate.

`bool *array;`
This array is used to keep track of whether the packet at the index has been received by the receiver or not. If the value at the index is false, then it means that packet is not received by the receiver, and therefore the sender needs to send that packet. Otherwise, the sender can skip that packet and move to check the next one. From the receiver side, if the value is false, then it has to write the content of that packet.

**The overall mechanism is:**
1. The sender reads the file and indexes the data into a packet.
2. The sender sends the packet to the receiver.
3. The receiver receives the packet from the sender.
4. The receiver writes the data into the write file.
5. The receiver sends back an ACK which is only composed of the index of the received packet.
6. The receiver marks the value in the array at that index as written (true).
7. The sender receives the ACK and marks the value in the array at that index as written (true).
8. The sender now will skip the true index and only send unmarked packets.
9. The receiver now will skip the true index and only write unmarked packets.

**This implementation ensures ordering and tolerates packet drop by:**
1. Sending initiation and termination packets multiple times to ensure high reliability via hardcoding.
2. Packets can be dropped through communication since if an ACK sent by the receiver is dropped, the sender will continue sending that packet until it receives an ACK.
3. The index and fixed buffer size make the write into the file in order.

**Test**

**E1 & E2:**
Check manually.

**E3:**
Run: `iperf3 -c <ipv4>` to get:
```
Connecting to host 128.110.223.37, port 5201
[  5] local 128.110.223.36 port 48668 connected to 128.110.223.37 port 5201
[ ID] Interval           Transfer     Bitrate         Retr  Cwnd
[  5]   0.00-1.00   sec  45.5 MBytes   382 Mbits/sec    0   1.08 MBytes       
[  5]   1.00-2.00   sec  27.5 MBytes   231 Mbits/sec    0   1.08 MBytes       
[  5]   2.00-3.00   sec  26.2 MBytes   220 Mbits/sec    0   1.08 MBytes       
[  5]   3.00-4.00   sec  26.2 MBytes   220 Mbits/sec    0   1.08 MBytes       
[  5]   4.00-5.00   sec  27.5 MBytes   231 Mbits/sec    0   1.08 MBytes       
[  5]   5.00-6.00   sec  27.5 MBytes   231 Mbits/sec    0   1.08 MBytes       
[  5]   6.00-7.00   sec  40.0 MBytes   336 Mbits/sec    0   1.08 MBytes       
[  5]   7.00-8.00   sec  26.2 MBytes   220 Mbits/sec    0   1.08 MBytes       
[  5]   8.00-9.00   sec  30.0 MBytes   252 Mbits/sec    0   1.08 MBytes       
[  5]   9.00-10.00  sec  28.8 MBytes   241 Mbits/sec    0   1.08 MBytes       
- - - - - - - - - - - - - - - - - - - - - - - - -
[ ID] Interval           Transfer     Bitrate         Retr
[  5]   0.00-10.00  sec   305 MBytes   256 Mbits/sec    0             sender
[  5]   0.00-10.05  sec   303 MBytes   253 Mbits/sec                  receiver
```
and using `iperf3` to get:
```
amdvm174-2.utah.cloudlab.  => amdvm174-1.utah.cloudlab.   222Mb   171Mb   143Mb
                           <=                               0b      0b      0b
amdvm174-2.utah.cloudlab.  => dhcp-128-189-28-214.ubcse  10.4Mb  8.02Mb  6.69Mb
                           <=                             264Kb   179Kb   149Kb
255.255.255.255            => 0.0.0.0                       0b      0b      0b
                           <=                               0b   1.56Kb  1.54Kb
amdvm174-2.utah.cloudlab.  => boss.utah.cloudlab.us       620b    124b    103b
                           <=                            1.14Kb   234b    195b
_gateway                   => all-systems.mcast.net       144b     29b     24b
                           <=                               0b      0b      0b

─bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb─
TX:             cum:    224MB   peak:    240Mb  rates:    233Mb   179Mb   150Mb
RX:                     227KB            265Kb            265Kb   181Kb   151Kb
TOTAL:                  224MB            241Mb            233Mb   180Mb   150Mb
```
We can see that with no limitation on bandwidth, the avg utilization rate is about 222Mb/250MB > 70%.

**E4:**
Sending and receiving from both sides yield the following bitrate:
```
amdvm174-2.utah.cloudlab.  => amdvm174-1.utah.cloudlab.  88.6Mb  47.9Mb  42.8Mb
                           <=                            84.0Mb  71.2Mb  51.3Mb
```
We can see they converge to roughly fairly sharing the link (88.6Mb is super close to 84.0Mb). The time is observed in 100RTTs.

**E5 & E6:**
Initializing a TCP-flow/UDP-implementation first, then start the other protocol to observe the bitrate using the same command as E3. By observation, they all have, on average, at least half as much throughput as the TCP-flow/UDP-implementation.

