Lab2 webserver for FHdortmund session nov2022 (SL, 18.11.22)

simple webserver for running on SAMD21

HW
    -) SAMD21-xpl 
    -) WINC1500 (connected to SAMD21-xpl EXT1 ext1)

Functionality:
 2) ...
 1) initial commit will (->git-tag 'L1basic_ telnet_0-1-?') functionality
    -) connect via telnet to winc1500
    -) send 
        '0' = LEDoff
        '1' = LEDon
        '?' = respond to webpage SW0=pressed/released
                depending on SW0-status
    SL, 18.11.22

#eof
