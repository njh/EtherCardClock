This Arduino sketch demonstrates getting the address of an NTP server from
a DHCP server and then querying those servers every 20 seconds for the time
and printing it to the serial port.

It is based on code stolen from Andrew Lindsay:
```https://github.com/thiseldo/EtherCardExamples/blob/master/EtherCard_ntp/EtherCard_ntp.ino```


The following Cisco IOS configuration was used to set the NTP server IP addresses:

    ip dhcp excluded-address 10.0.0.0 10.0.0.100

    ip dhcp pool lan
     network 10.0.0.0 255.255.255.0
     domain-name example.com
     dns-server 217.169.20.20 217.169.20.21
     default-router 10.0.0.1
     option 42 ip 90.155.53.93 90.155.53.94 
     lease 14 

    
The same can be done using the ISC DHCP Server with:

    option ntp-servers 90.155.53.93, 90.155.53.94;

