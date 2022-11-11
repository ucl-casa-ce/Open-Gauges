import requests
import json
import time
import neopixel
import board


#Set Colours

RED = (255, 0, 0)
YELLOW = (255, 150, 0)
ORANGE = (100, 64, 0)
GREEN = (0, 255, 0)
CYAN = (0, 255, 255)
BLUE = (0, 0, 255)
PURPLE = (180, 0, 255)
OFF = (0, 0, 0)



#Set NeoPixel Details - Pin/Number Pixels/Brightness etc
pixels = neopixel.NeoPixel(board.D18, 144, brightness=0.03, auto_write=False)


#Start up Lights

n = 1

t_end = time.time() + 22.32 * 1
while time.time() < t_end:
    n = n + 1
    if n >= 144:
        n = 1
    pixels[n] = (RED)
    pixels[n-1] = (YELLOW)
    pixels.show()
    time.sleep (0.1)
    
pixels.fill((0, 0, 0))

pixels.show()




print ("Getting Conditions and Forecast")

def getconditions():

# Get Data from Weather Flow for Station Location

    try:
        response = requests.get('https://swd.weatherflow.com/swd/rest/better_forecast?api_key=db55228a-b708-4325-9166-7f2d04c61baa&station_id=50216&units_temp=c&units_wind=mph&units_pressure=mb&units_precip=mm&units_distance=mi').text
    except requests.exceptions.RequestException as e: 
        time.sleep(60)

    data = json.loads(response)

    text = data['current_conditions']['conditions']
    icon = data['current_conditions']['icon']
    baro = int(data['current_conditions']['sea_level_pressure'])
    trend = data['current_conditions']['pressure_trend']

    print(text)
    print(icon)
    print(baro)
    print(trend)

    
    return trend, baro, icon
    

def barometer():
    conditions = getconditions()
    baro = conditions[1]

    # Pressure top 1050 minus number of pixels to set top pixel
    pixel = 906

    pixelon = int(baro - pixel)
    pixels[pixelon] = (RED)
    

    
def trendpixel():
    conditions = getconditions()
    trend = conditions[0]


    if trend == 'steady':
     pixels[14] = (GREEN)
    else:
     pixels[14] = (OFF)

    if trend == 'rising':
     pixels[16] = (BLUE)
    else:
     pixels[16] = (OFF) 

    if trend == 'falling':
     pixels[12] = (RED)
    else:
     pixels[12] = (OFF) 

def icon():
    conditions = getconditions()
    icon = str(conditions[2])
    print("Icon")
    print(icon)

    if icon == 'clear-day':
     pixels[36] = (YELLOW)
    else:
     pixels[36] = (OFF)

    if icon == 'partly-cloudy-day' or 'partly-cloudy-night':
     pixels[34] = (BLUE)
    else:
     pixels[34] = (OFF)

    if icon == 'cloudy':
     pixels[32] = (BLUE)
    else:
     pixels[32] = (OFF) 

    if icon == 'possibly-rainy-day':
     pixels[30] = (BLUE)
    else:
     pixels[30] = (OFF)

    if icon == 'possibly-rainy-night':
     pixels[30] = (BLUE)
    else:
     pixels[30] = (OFF) 

    if icon == 'clear-night':
     pixels[22] = (BLUE)
    else:
     pixels[22] = (OFF) 



while True:
    getconditions()
    barometer()
    trendpixel()
    icon()
    pixels.show()
    time.sleep(60)
