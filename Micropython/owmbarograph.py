import time
import network
import machine
from machine import Pin, PWM
import urequests
from time import sleep

ssid = 'Your SSID'
password = 'Your PASSWORD'

global baro
global servodegrees


# Set up Servo Pins and Data Range
servoPin = PWM(Pin(16))
servoPin.freq(50)

servorange = 25


wlan = network.WLAN(network.STA_IF)
wlan.active(True)

def connect():
    wlan.connect(ssid, password)
    # Wait for connect or fail
    max_wait = 10
    while max_wait > 0:
        if wlan.status() < 0 or wlan.status() >= 3:
            break
        max_wait -= 1
        print('waiting for connection...')
        time.sleep(1)

# Handle connection error
if wlan.status() != 3:
    raise RuntimeError('network connection failed')
    connect()
else:
    print('connected')
    status = wlan.ifconfig()
    print( 'ip = ' + status[0] )
    
##
    
#Set Open Weather Map API

def get_conditions():
    
    global baro

   
    print ("Getting Data from Open Weather Map")

    api_key = "Yours API KEY"
    lat = "Your Lat"
    lon = "Your Long"
    url = "https://api.openweathermap.org/data/2.5/onecall?lat=%s&lon=%s&appid=%s&units=metric" % (lat, lon, api_key)
    response = urequests.get(url)
    data = response.json()
    baro = int(data ["current"]["pressure"])
    print("Current Pressure = ", baro)
    

def translate():
    global baro
    global servodegrees
    
    # Set Data Range to Remap

    leftMin = 950
    leftMax = 1050
    rightMin = 0
    rightMax = servorange
    #Calculate Range
    leftSpan = leftMax - leftMin
    rightSpan = rightMax - rightMin
    
    #Convert range to float
    valueScaled = float(baro - leftMin) / float(leftSpan)
    #Create Range
    servodegrees = rightMin + (valueScaled * rightSpan)


def servo(degrees):
    # limit degrees beteen 0 and 180
    if degrees > 180: degrees=180
    if degrees < 0: degrees=0
    # set max and min duty
    #Reverse order below to change direction according to servo setup
    maxDuty=1000
    minDuty=9000
    
    # new duty is between min and max duty in proportion to its value
    newDuty=minDuty+(maxDuty-minDuty)*(degrees/180)
    # servo PWM value is set
    servoPin.duty_u16(int(newDuty))


# First Sweep - Degree Range to be Edited According to Servo for Setup

def sweep():
    n= 0
    while n < servorange :
      
        servo(n)
        sleep(servospeed)
        n = n+1
        
    sleep(5)    
    n = servorange
    while n >= 1 :
      
        servo(n)
        sleep(servospeed)
        n = n-1
    sleep(5)
        
def moveservo():
    
    global baro
    global servodegrees

#run the data through the translate function to remap range  
    translate()
    servo(servodegrees)
    print("Moving Servo, ", servodegrees, " Degrees")
    sleep(1)
 
while True:
    get_conditions()
    moveservo()
    print("Waiting for next data request - Set for every 15 mins")
    time.sleep(900) #time to wait, in seconds, before getting data again - Pico could be put into deep sleep mode
    
else:
    connect()