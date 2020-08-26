# Lab-on-Computer-Network
Lab on "Computer Networks", Spring 2018, Peking University. Several programming tasks based on [NetRiver System](https://www.researchgate.net/publication/220784376_NetRiver_a_computer_network_experiment_system). 

* [hw1 Slide Window](hw1_slide_window.cpp): slide window protocol including one-bit, go back N and selective repeat protocol.
* [hw2 ipv4 packet processing](hw2_ipv4.cpp): 
- IPv4 packet receiving: validation(version, TTL, header length, distination address), action(discard/send)
- IPv4 packet sending: checkSum calculation, network byte order transformation, packet encapsulation and transmission.
* [hw3 ipv4 packet routing and forwarding](hw3.cpp): IPv4 packet routing and forwarding
* [hw4 tcp client-end simulator](hw4_tcp.cpp): TCP client-end simulator using Stop-and-Wait protocol.
