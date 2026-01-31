This project is a complex console C++ project working on Raspberry PI 5.
This project consists of several functional blocks each block should use its own thread if this way is the best, 
if not, you can join some blocks  in a single thread.
Communications between threads should be via signals or messages.

Code should meet the best practice with C++ programming
Code should meet thread safe practices

Device may have 1 or 2 video cameras.
Door0_Open, Door0_Close, Cam0ON, Cam0OFF, Light0ON, Light0OFF refers to Camera0
Door1_Open, Door1_Close, Cam1ON, Cam1OFF, Light1ON, Light1OFF refers to Camera1


  
The functional blocks are:
1. Block named MainControll.
	It is responsible of ommunication via USB Serial with periferal devies
	It searchs ttyUSB devices in system and find one based on CH340 chip.
	Establish connection with external device via USB_Serial discovered at previous step 
	on speed 115200 boud
	It getting commands from periferals ans sends commands (via inter thread messages) to other 
	functional blocks.
	Each command getting from periferal is stored to log file with datetime stamp. 
	Log file located in ~/PassFlow/Log 
	Commands getting from periferal are:
		1.1.1. Door0_Open
		1.1.2. Door0_Close
		1.1.3. Door1_Open
		1.1.4. Door1_Close
		1.1.5. MainSupplayON
		1.1.6. MainSupplyOFF
		1.1.7. IgnitionON
		1.1.8. IgnitionOFF
		1.1.9. Cover0Opened
		1.1.10. Cover0Closed
		1.1.11. Cover1Opened
		1.1.12. Cover1Closed
	Commands sending to periferals are:
		1.2.1. RedLedON
		1.2.2. RedLedOFF
		1.2.3. RedLedBlink
		1.2.4. GreenLedON
		1.2.5. GreenLedOFF
		1.2.6. GreenLedBlink
		1.2.7. BlueLedON
		1.2.8. BlueLedOFF
		1.2.9. BlueLedBlink
		1.2.10. Cam0ON
		1.2.11. Cam0OFF
		1.2.12. Cam1ON
		1.2.13. Cam1OFF
		1.2.14. Light0ON
		1.2.15. Light0OFF
		1.2.16. Light1ON
		1.2.17. Light1OFF
		1.2.18. FunON
		1.2.19. FunOff
	
	Getting commands Door0Open or Door1Open fix tamistamp of event
	Getting commands Door0Close or Door1Close fix tamistamp of event and 
	send message "StartStop" with two datetime stamps (DoorOpen and DoorClosed) to
	VideoControll block for corresponding camera
	Description of other commands I will give later. 
	
2. Block named VideoControll
From starting the main application, checks settings and connect to IP camera via Ethernet 
and record stream to file via ffmpeg. If settings says read stream from two cameras, connetc to two cameras.
It cam be 1 thread serving 2 cameras, or one thread for each camera.
This file name consists of datetime of starting and suffix "cam0(1)"
depends on what cameras stream is.
Records from cameras located in ~/PassFlow/Cam0Source ans ~/PassFlow/Cam1Source
When message "StartStop" from MainControll recieved  VideoControll block stops writing current video file
and starts a new one.
Then from closed file VideoControll cuts fragment from Start datetime to Stop datetime (from  "StartStop" message), 
change color and resize it and locate in ~/PassFlow/Cam0/day  or ~/PassFlow/Cam1/day respectively. 
Where "day" is current date. If the folder with "date" name not exist, create it.


4. commands are:
	4.1. Door0_Open
	4.2. Door0_Close
	4.3. Door1_Open
	4.4. Door1_Close
	4.5. MainSupplayON
	4.6. MainSupplyOFF
	4.7. IgnitionON
	4.8. IgnitionOFF
	4.9. Cover0Opened
	4.10. Cover0Closed
	4.11. Cover1Opened
	4.12. Cover1Closed
5. this block sends via USB_Serial to periferal commands each command ia 1 byte lenght:
	5.1. RedLedON
	5.2. RedLedOFF
	5.3. RedLedBlink
	5.4. GreenLedON
	5.5. GreenLedOFF
	5.6. GreenLedBlink
	5.7. BlueLedON
	5.8. BlueLedOFF
	5.9. BlueLedBlink
	5.10. Cam0ON
	5.11. Cam0OFF
	5.12. Cam1ON
	5.13. Cam1OFF
	5.14. Light0ON
	5.15. Light0OFF
	5.16. Light1ON
	5.17. Light1OFF
	5.18. FunON
	5.19. FunOff
	
6. After recieving command from periferal 	Door0_Open and Door0_Close 