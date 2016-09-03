from time import sleep
import serial
import eeml

API_KEY = 'HajSu78jsuNyxBo9knFvLt40FBIviUBHUexjlFxRf3JJZbGJ'
FEED = 1096835160
API_URL = '/v2/feeds/{feednum}.xml' .format(feednum = FEED)

port = "/dev/ttyUSB2"
baud = 115200
ser = serial.Serial(port, baud, timeout=0)

while True:
    # create an empty list 
    myList = []
    # read from serial
    data = ser.read(9999)
    # split the data at commas
    # data is in the format: humidity(%RH),temperature(c),height(cm)
    myList = data.split(",")
    if len(myList) == 3:
        # open feed
        pac = eeml.Pachube(API_URL, API_KEY)
        # compile data
        pac.update([eeml.Data("height", myList[2], unit=eeml.Unit(name='height', type="contextDependentUnits", symbol='cm'))])
        pac.update([eeml.Data("temperature", myList[1], unit=eeml.Celsius())])
        pac.update([eeml.Data("humidity", myList[0], unit=eeml.RH())])
        # send the data
        try:
            pac.put()
        except:
            print "update failed"
    sleep(0.5)

ser.close()
