import serial
import math
import time 
from time import sleep, strftime
from subprocess import *
from datetime import datetime
import collections
import eeml
import smtplib
from email.mime.text import MIMEText

mailUser = "matrixw@mailworks.org"
mailPwd = "mk,njw3esaq2"
mailTo = "alex@matrixwide.com"

# CPM alert level
CPM_ALERT = 50

# flag if an email was sent to prevent sending one every second
# we will reset this flag every hour
mailSent = 0

sleep(1)

API_KEY = 'mGZ5SJPBDGYTGctrIUv8y9jVlgUTaWLQZeN2ZQDnxWv2Ktyo'
FEED_ID = 423160622
API_URL = '/v2/feeds/{feednum}.xml' .format(feednum = FEED_ID)

# delay for network services startup
time.sleep(30)

port = "/dev/ttyUSB3"
baud = 9600
ser = serial.Serial(port, baud, timeout=10)
count = 0
hourCount = 0
total = 0
usvh = 0
lnd712Conversion = 0.00812
d = collections.deque([0] * 60)
logfile = "/var/log/radlog"

# open the log in append mode
f = open(logfile,"a")

def run_cmd(cmd):
    p = Popen(cmd, shell=True, stdout=PIPE)
    output = p.communicate()[0]
    return output

while True:
    try:
        data = ser.readline()
        data = int(data)

        # pop off a reaing
        d.popleft()

        # append the new reading
        d.append(data)
        
        # the queue contains 60 seconds worth of CPS data
        # so the sum of the queue is the CPM
        total = sum(d)
        usvh = float(total) * lnd712Conversion

        count = count + 1
        hourCount = hourCount + 1

        # if total is greater than or equal to CPM_ALERT
        # send an email with the CPM if email hasn't been sent in the last hour
        if total >= CPM_ALERT and mailSent == 0:
            mailSent = 1
            msg = MIMEText('CPM was: %d' % (total))
            msg['Subject'] = 'Radiation Threshold Alert'
            msg['From'] = mailUser
            msg['To'] = mailTo
            server = smtplib.SMTP('mail.messagingengine.com:587')
            server.ehlo_or_helo_if_needed()
            server.starttls()
            server.ehlo_or_helo_if_needed()
            server.login(mailUser,mailPwd)
            server.sendmail(mailUser, mailTo, msg.as_string())
            server.quit()
        
        # at one minute, upload the CPM to xively
        # and log the CPM to a file
        if count == 60:
            # reset the count
            count = 0
            skip = 0 
            # upload to xively
            # open feed
            try:
                pac = eeml.Pachube(API_URL, API_KEY)
            except:
                print ("error connecting to xively feed")
                skip = 1
            if skip == 0:
                pac.update([eeml.Data("count", total, unit=eeml.Unit(name='count', type="contextDependentUnits", symbol='#'))])
                pac.update([eeml.Data("uSvh", usvh, unit=eeml.Unit(name='sievert', type="derivedSI", symbol='S'))])
                # send the data
                try:
                    pac.put()
                except:
                    print "update failed"

        # at one hour, log the CPM to the logfile
        if hourCount == 3600:
            # reset the count
            hourCount = 0
            # reset the emailSent flag
            mailSent = 0
            # log to file date/time and cpm
            now = int(time.time())
            try:
                f.write(str(now) + "," + str(total) + "\n")
                f.flush()
            except:
                print "Unable to write to logfile %s" % ( logfile )


        sleep(0.5)

    except KeyboardInterrupt:
        print ('exiting')
        break
    except ValueError:
        pass
ser.flush()
ser.close()
