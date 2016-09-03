import serial
from time import sleep
import eeml

API_KEY = 'ytdMJP0LGpkOD7oAx86yzmF531nFdgv91Qr6d3l3SxE2pUxc'
FEED = 1889157289
API_URL = '/v2/feeds/{feednum}.xml' .format(feednum = FEED)

port = "/dev/ttyACM0"
baud = 9600
ser = serial.Serial(port, baud, timeout=0)
outfile = "/var/tmp/wattstemphumid"

while True:
    # create an empty list 
    myList = []
    # read from serial
    data = ser.read(9999)
    # split the data at commas
    # data is in the format: watthours,fahrenheit,humidiity
    myList = data.split(",")
    if len(myList) == 3:
        # open feed
        pac = eeml.Pachube(API_URL, API_KEY)
        # compile data
        pac.update([eeml.Data("Watthours", myList[0], unit=eeml.Unit(name='watt', type='derivedSI', symbol='W'))])
        pac.update([eeml.Data("Temperature", myList[1], unit=eeml.Fahrenheit())])
        pac.update([eeml.Data("Humidity", myList[2], unit=eeml.RH())])
        # send the data
        try:
            pac.put()
        except:
            print "update failed"
        # write data to file
        try:
            f = open(outfile,"w")
            f.write(myList[0] + "," + myList[1] + "," + myList[2])
            f.flush()
            f.close()
        except:
            print "Unable to write to file %s" % (outfile) 
    sleep(0.5)

ser.close()
