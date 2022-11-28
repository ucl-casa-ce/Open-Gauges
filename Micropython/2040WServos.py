# clean.py Test of asynchronous mqtt client with clean session.
# (C) Copyright Peter Hinch 2017-2019.
# Released under the MIT licence.

# Public brokers https://github.com/mqtt/mqtt.github.io/wiki/public_brokers

# The use of clean_session means that after a connection failure subscriptions
# must be renewed (MQTT spec 3.1.2.4). This is done by the connect handler.
# Note that publications issued during the outage will be missed. If this is
# an issue see unclean.py.

# red LED: ON == WiFi fail
# blue LED heartbeat: demonstrates scheduler is running.


#Speed of the servo.value movement - 0.05 provides a good smooth speed
from neopixel import Neopixel
from mqtt_as import MQTTClient, config
from config import wifi_led, blue_led  # Local definitions
import uasyncio as asyncio
import machine
from machine import Pin, PWM
from time import sleep
import json
from inventor import Inventor2040W, NUM_SERVOS, SERVO_1, SERVO_2, SERVO_3, GPIOS, NUM_GPIOS

# Set up neopix

def pixels():
#Set up Neopixels
    numpix = 53 #number of neopixels to the sweep the servo

    pixels = Neopixel(numpix, 4, 0, "RGB")

    WHITE = (5, 5, 5) #255 is full brightness - not recommended due to power draw
    pixels.fill(WHITE)
    pixels.show()   
    time.sleep(2)
   

# Create a new Inventor2040W and get a servo.value from it

board = Inventor2040W()
windservo = board.servos[SERVO_3]
tempservo = board.servos[SERVO_1]
pressureservo = board.servos[SERVO_2]

global wind
global windservodegrees
global temp
global tempservodegrees
global pressure
global pressureservodegrees



#Set up first reading for Tween

servospeed = 0.05
winddata = [60,60]


# First Sweep - Degree Range to be Edited According to servo.value and Gear Ratio
def sweep():
    windservo.value(0)
    tempservo.value(0)
    pressureservo.value(0)
    
    n= -90
    while n < 90 :
      
        windservo.value(n)
        tempservo.value(n)
        pressureservo.value(n)
        sleep(servospeed)
        n = n+1
        
    sleep(4)    
    n= 90
    while n >= -90 :
      
        windservo.value(n)
        tempservo.value(n)
        pressureservo.value(n)
        sleep(servospeed)
        n = n-1
            

 


# Subscription callback
def sub_cb(topic, msg, retained):
    print(f'Topic: "{topic.decode()}" Message: "{msg.decode()}" Retained: {retained}')
    
    global wind
    global temp
    global pressure
    
    topic=msg
    m_decode=str(msg.decode("utf-8","ignore"))
    print("data Received type",type(m_decode))
    print("data Received",m_decode)
    print("Converting from Json to Object")
    data=json.loads(m_decode) #decode json data
    print(type(data))
    wind = int(data["windSpeed_mph"])
    temp = float(data["outTemp_C"])
    pressure = float(data["barometer_mbar"])
    print("Wind = " , wind)
    print("Temp = ", temp)
    print("Pressure = ", pressure)
    
    
    translatewind()
    windservofunction()
    translatetemp()
    tempservofunction()
    translatepressure()
    pressureservofunction()

def translatewind():
    global windservodegrees
    global wind
    leftMin = 0
    leftMax = 40
    rightMin = 73
    rightMax = -71
    #Calculate Range
    leftSpan = leftMax - leftMin
    rightSpan = rightMax - rightMin
    
    #Convert range to float
    valueScaled = float(wind - leftMin) / float(leftSpan)
    #Create Range
    windservodegrees = rightMin + (valueScaled * rightSpan)


def translatetemp():
    global tempservodegrees
    global temp
    leftMin = -10
    leftMax = 40
    rightMin = 90
    rightMax = -90
    #Calculate Range
    leftSpan = leftMax - leftMin
    rightSpan = rightMax - rightMin
    
    #Convert range to float
    valueScaled = float(temp - leftMin) / float(leftSpan)
    #Create Range
    tempservodegrees = rightMin + (valueScaled * rightSpan) 
 
def translatepressure():
    global pressureservodegrees
    global pressure
    leftMin = 950
    leftMax = 1050
    rightMin = 90
    rightMax = -90
    #Calculate Range
    leftSpan = leftMax - leftMin
    rightSpan = rightMax - rightMin
    
    #Convert range to float
    valueScaled = float(pressure - leftMin) / float(leftSpan)
    #Create Range
    pressureservodegrees = rightMin + (valueScaled * rightSpan)
 
def tempservofunction():
    global temp
    tempservo.value(tempservodegrees)
    
def pressureservofunction():
    global pressure
    pressureservo.value(pressureservodegrees)
        
def windservofunction():
    global wind
    sleep(servospeed)
    winddata.append(windservodegrees)
    windservo.value(windservodegrees)
   
    if winddata[0] == winddata[-1]:
           
                sleep(2)      
    else:
        
        while winddata[0] >  winddata[-1]:
            windservo.value(winddata[0])
           
            sleep(servospeed)
            winddata[0] = (winddata[0])-1
            
            if winddata[0] == winddata[-1]:
          
                sleep(2)         
         
        while winddata[0] <  winddata[-1]:
            windservo.value(winddata[0])
            sleep(servospeed)
            winddata[0] = (winddata[0])+1
            
            if winddata[0] == winddata[-1]:
                sleep(2)
     
    
    if len(winddata) > 3:
        print ("Trimming List")
        del winddata[3]      
           
        print(winddata)
        
       
          
   
      
    
# Demonstrate scheduler is operational.
async def heartbeat():
    s = True
    while True:
        await asyncio.sleep_ms(500)
        blue_led(s)
        s = not s

async def wifi_han(state):
    wifi_led(not state)
    print('Wifi is ', 'up' if state else 'down')
    sweep()
    #pixels()
   
    await asyncio.sleep(1)

# If you connect with clean_session True, must re-subscribe (MQTT spec 3.1.2.4)
async def conn_han(client):
    await client.subscribe('personal/ucfnaps/downhamweather/loop', 1)

async def main(client):
    try:
        await client.connect()
    except OSError:
        print('Connection failed.')
        return
    n = 0
    while True:
        await asyncio.sleep(5)
       # print('publish', n)
        # If WiFi is down the following will pause for the duration.
        #await client.publish('result', '{} {}'.format(n, client.REPUB_COUNT), qos = 1)
        n += 1

# Define configuration
config['subs_cb'] = sub_cb
config['wifi_coro'] = wifi_han
config['connect_coro'] = conn_han
config['clean'] = True

# Set up client
MQTTClient.DEBUG = True  # Optional
client = MQTTClient(config)

asyncio.create_task(heartbeat())

try:
    asyncio.run(main(client))
    windservofunction()

finally:
    client.close()  # Prevent LmacRxBlk:1 errors
    asyncio.new_event_loop()