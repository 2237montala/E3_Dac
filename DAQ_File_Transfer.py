import serial
from time import sleep

##Open com port
portNum = "/dev/ttyUSB0"
baudRate = 115200
transfer = False #If true it delete the csv files on the sd card
delete = True

endLineChar = 'ss'
numFileTrans = 0


ser = serial.Serial(port="/dev/ttyUSB0",baudrate = baudRate,timeout = 1,write_timeout=1,bytesize=serial.EIGHTBITS,parity=serial.PARITY_NONE,stopbits=serial.STOPBITS_ONE)
##Option are
## t - transfer all csv files
## d - delete all csv files
sleep(2)
print("Writing start command")
if(transfer):
    ser.write(b't')
    print("Reading")
    #test = ser.readline()
    #print(test)
    sleep(2)
    #Get begining word
    test = ser.readline()
    print(test)
    while(not ("//" in endLineChar)):
        #fileName = ser.read(13)
        fileName = ser.readline()
        fileName = fileName[:-2]
        if(not ("data" in fileName)):
            break
        numFileTrans +=1
        fw = open(fileName,"w")
        print(fileName)
        endLineChar = "  "
        while(not( "/" in endLineChar)):
            data = str(ser.readline())
            endLineChar = data
            if(not("/" in endLineChar)):
                fw.write(data)
        fw.close()
    if(numFileTrans == 0):
        print("No files transfered")
    else:
        print("Number of files transfered: %d" % numFileTrans)



if(delete):
    ser.write(b'd')
    sleep(0.5)
    #Get deleting starting word
    status = ser.readline()
    print(status)
    sleep(2)
    #Get deleting ending word
    status = ser.readline()
    print(status)


ser.close()
