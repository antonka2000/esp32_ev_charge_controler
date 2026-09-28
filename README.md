Welcome to the esp32 ev charge project.

What it can do as of 09/26

1.) I can read pv data from Sungrow SH10RT;  
see how much i can provide to a Keba PV 30C;  
put this as the charging value for a connected car.
2.) minimum charging combined with PV-Charging. 
3.) make minimum charging 
4.) provice maximum charging
5.) this option is still "work in progress"

all in a simple esp32 via modbus tcp via wifi  

Perspectives and goals:  

#1  

4 modes  

1- PV-mode  
2- min charge mode (+PV) - Min Value to set via web interface  
3- max charge mode       - Max to set via web interface  
4- eco charge mode       - set via web interface:  
        - start time  
        - finish time  
        - charge power  
        - use min cost values from awattar ( or similar systems)  

#2  
mode changes via button  
mode indications via 4 leds  
fast blink for successful charging
slow blink when charging stopped

#3  

different chargers   / planed
different inverters  / planed

#4  
keep it simple, costless, user friendly  
So email what you want and help improve the system.  

<img width="660" height="886" alt="grafik" src="https://github.com/user-attachments/assets/a5c4cca3-301e-4833-aa9b-5dfd4854dd2e" />



      
