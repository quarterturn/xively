import serial
from time import sleep
import eeml

API_KEY = 'ytdMJP0LGpkOD7oAx86yzmF531nFdgv91Qr6d3l3SxE2pUxc'
FEED = 1889157289
API_URL = '/v2/feeds/{feednum}.xml' .format(feednum = FEED)

port = "/dev/ttyACM0"
baud = 9600
ser = serial.Serial(port, baud, timeout=0)

while True:
    data = ser.read(9999)
    if len(data) > 0:
        # open up your feed
        pac = eeml.Pachube(API_URL, API_KEY)
        # compile data
        pac.update([eeml.Data("watthours", data, unit=eeml.Unit(name='watt', type='derivedSI', symbol='W'))])
        # send the data
        try:
            pac.put()
        except:
            print "pachube update failed"
    sleep(0.5)

ser.close()
