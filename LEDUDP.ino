#include <ESP8266WiFi.h>
#include <FastLED.h>
//#include <Arduino.h>
#include <WiFiUdp.h>
//#include <math.h>
#include <EEPROM.h>

#define PIN D6
#define COUNTLED 60
#define WAITDELAY 10

#ifndef STASSID
#define STASSID "Birn-Wlan"
#define STAPSK "01965252530146103052"
#endif

#define DEFAULT_globalBrightness 50
#define DEFAULT_globalcolortype 0
#define DEFAULT_globallightlength 3
#define DEFAULT_globaldecay 80
#define DEFAULT_globalTimeDelay 25
#define DEFAULT_globalOnlyOne 0
#define DEFAULT_globalPcMode 0
#define DEFAULT_globalSlwoFading 20
#define DEFAULT_globalSlwoFadingTime 20
#define DEFAULT_globalTransitionOn 1

//UDP STUFF
WiFiUDP Udp;
unsigned int localUdpPort = 4210; // local port to listen on
// ip .190 = port 4210 / ip .191 = 4211 / ip .193 = 4212
char incomingPacket[255];   // buffer for incoming packets
char replyPacket[] = "ACK"; // a reply string to send back

//WIFI
const char *ssid = STASSID;
const char *password = STAPSK;
const char *hostname = "LEDLamp";
int status = WL_IDLE_STATUS;

//On/Off
boolean isOn = false;

//LED SETTINGS
byte animation = 0;
/*
    /0 = static color
    /1 = changeColorToNext()
    /2 = drittel
    /3 = runningRainbow
    /4 = bounce
    /5 = light from middle to edges
    /6 = Stack
    /7 = meteorRain
    /8 = twinkle
    /9 = ampelMode
*/

//Colors
// long colorsOld[3] = {0,0,0};
// long colorsNext[3] = {0,0,0};
CRGB colorsOld[3] = {CRGB(255, 0, 0), CRGB(0, 255, 0), CRGB(0, 0, 255)};
CRGB colorsNext[3] = {CRGB(255, 0, 0), CRGB(0, 255, 0), CRGB(0, 0, 255)};
CRGB colorsBackup[3] = {CRGB(255, 0, 0), CRGB(0, 255, 0), CRGB(0, 0, 255)};
byte hue = 0;

byte globalVariables[] = {50, 0, 3, 80, 25, 0, 0, 20, 20, 1};

/*
    /0 = globalBrightness
        /int = Brightness
    /1 = globalcolortype
        /0 = Color 1
        /1 = Fading Color, Changing from 1 to 2 and back
        /2 = Rainbow
    /2 = globallightlength
        /int = length
    /3 = globaldecay
        /int = decay
    /4 = globalTimeDelay
        /int = delay
    /5 = globalOnlyOne
        /0 = false
        /1 = true
    /6 = globalOnlyOneRun
        /0 = false
        /1 = true
    /7 = globalSlwoFading
        /int = slowFading
    /8 = globalSlwoFadingTime
        / = slowFadingTime
    /9 = transitionOn
        /0 = off
        /1 = on
*/
// byte globalColorType = 0;
// byte globalLightLength = 3;
// int globalDecay = 80;
// int timedelay = 25;
// int bright =  255;
// bool globalOnlyOne = false;
// bool globalOnlyOneRun = false;
// int slowFadingCount = 50;

boolean colorPrevMode = false;

//Programm Varibles
boolean updateVariables = true;
boolean colorsChanged = true;
byte iterration = 0;
byte currentPos = 0;
int connectTrys = 0;
unsigned long lastTimeAnimation = -100;
unsigned long lastTimeWIFI = -100;

//Debug Varibles
long debugStartTime = 0;
long debugEndTime = 0;

//LED ARRAY
CRGB leds[COUNTLED]; //Actuall LED ARRAY

//TIMER
extern "C"
{
#include "user_interface.h"
}
os_timer_t myTimer;
bool tickOccured;

//////////////////////////////////////////////////////////////////////
///////////////////////////// SETUP //////////////////////////////////
//////////////////////////////////////////////////////////////////////

void setup()
{

    Serial.begin(9600);
    Serial.setDebugOutput(true);

    EEPROM.begin(512);

    loadEEPROM();
    
    if (WiFi.status() == WL_NO_SHIELD)
    {
        Serial.println("WiFi shield not present, stopping Programm");
        while (true)
            ; // don't continue
    }

    FastLED.addLeds<NEOPIXEL, PIN>(leds, COUNTLED);
    //fill_solid(leds, COUNTLED, CRGB(255, 0, 0));

    WiFi.persistent(true);
    wificonnect();

    initGlobalVariables();
}

//////////////////////////////////////////////////////////////////////
///////////////////////////// EEPROM //////////////////////////////////
//////////////////////////////////////////////////////////////////////

void loadEEPROM()
{
    Serial.println("EEPROM loading...");

    EEPROM.get(0, isOn);
    Serial.println("EEPROM loading Colors...");
    for (int i = 0; i < 3; i++)
    {
        Serial.print("EEPROM loading Color number:");
        Serial.println(i);

        long colorLong = 0;
        EEPROM.get(1 + i * 4, colorLong);

        Serial.print("ColorNext number:");
        Serial.print(i);
        Serial.print(" set to:");
        Serial.println(colorLong);
        CRGB color = colorLong;
        colorsNext[i] = color;
    }

    EEPROM.get(13, animation);

    Serial.println("EEPROM loading globalVariables...");
    for (int i = 0; i < 9; i++)
    {
        Serial.print("EEPROM loading globalVariables number:");
        Serial.println(i);

        byte value = 0;
        EEPROM.get(14 + i, value);

        Serial.print("globalVariable number:");
        Serial.print(i);
        Serial.print(" set to:");
        Serial.println(value);
        globalVariables[i] = value;
    }

    Serial.println("EEPROM loaded");
    colorsChanged = true;
}

void saveEEPROM()
{

    boolean needToCommit = false;

    Serial.println("Saving...");
    boolean tmpOn = EEPROM.get(0, tmpOn);
    if (tmpOn != isOn)
    {
        EEPROM.put(0, isOn);
        needToCommit = true;
    }

    // Serial.println("EEPROM Saving Colors...");
    for (int i = 0; i < 3; i++)
    {
        long tmpColor = EEPROM.get(1 + i * 4, tmpColor);
        long tmpCurrentColor = 0;
        tmpCurrentColor = colorsNext[i].b;
        tmpCurrentColor += colorsNext[i].g * 256;
        tmpCurrentColor += colorsNext[i].r * 65536;

        // CRGB color = tmpColor;
        if (tmpCurrentColor != tmpColor)
        {
            Serial.print("EEPROM Saving Color number:");
            Serial.println(i);
            Serial.print("EEPROM Color was:");
            Serial.println(tmpColor);
            Serial.print("Current Color was:");
            Serial.println(tmpCurrentColor);

            EEPROM.put(1 + i * 4, tmpCurrentColor);
            needToCommit = true;
        }
    }

    byte tmpAnimation = EEPROM.get(13, tmpAnimation);
    if (tmpAnimation != animation)
    {
        EEPROM.put(13, animation);
        needToCommit = true;
    }

    // Serial.println("EEPROM Saving globalVariables...");
    for (int i = 0; i < 9; i++)
    {
        byte tmpValue = EEPROM.get(14 + i, tmpValue);
        if (tmpValue != globalVariables[i])
        {
            Serial.print("EEPROM Saving globalVariables number:");
            Serial.println(i);
            EEPROM.put(14 + i, globalVariables[i]);
            needToCommit = true;
        }
    }

    if (needToCommit)
    {
        Serial.println("Commiting!!!");
        EEPROM.commit();
        Serial.println("Done Saving Values to EEPROM!");
    }
    else
    {
        Serial.println("No need to save to EEPROM!");
    }
}

void initGlobalVariables()
{
    FastLED.setBrightness(globalVariables[0]);
    FastLED.show();
}

///////////////////////////// WIFI SETUP /////////////////////////////

void wificonnect()
{
    if (connectTrys == 0)
    {
        Serial.print("Connecting to "); // Connect WiFi
        Serial.println(ssid);

        /* Explicitly set the ESP8266 to be a WiFi-client, otherwise, it by default,
        would try to act as both a client and an access-point and could cause
        network-issues with your other WiFi-devices on your WiFi-network. */

        Serial.printf("Wi-Fi mode set to WIFI_STA %s\n", WiFi.mode(WIFI_STA) ? "" : "Failed!");
        WiFi.setSleepMode(WIFI_NONE_SLEEP);
        WiFi.disconnect();

        Serial.print("Scan start ... ");
        int n = WiFi.scanNetworks();
        Serial.print(n);
        Serial.println(" network(s) found");
        for (int i = 0; i < n; i++)
        {
            Serial.println(WiFi.SSID(i));
        }
        Serial.println();

        WiFi.setAutoReconnect(false);

        WiFi.hostname(hostname);
    }

    connect();

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println();
        Serial.println("WiFi connected");

        // Print the IP address
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());

        Udp.begin(localUdpPort);
        Serial.printf("Now listening at IP %s, UDP port %d\n", WiFi.localIP().toString().c_str(), localUdpPort);

        Serial.println("--------------------------");
        Serial.println("LED UDP Control");
        Serial.println("--------------------------");
    }
}

void connect()
{

    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.printf("  %d try => status [%d]\n", connectTrys, WiFi.status());
        Serial.flush();
        if (connectTrys >= 100)
        {
            saveEEPROM();
            Serial.println("\nReboot");
            ESP.reset();
        }
        else if (connectTrys % 10 == 0 || connectTrys == 0)
        {
            WiFi.begin(ssid, password);
        }

        connectTrys++;
    }
}

//////////////////////////////////////////////////////////////////////
//////////////////////// newReq ////////////////////////////
//////////////////////////////////////////////////////////////////////

void newReq()
{

    //  Serial.println("Checking for Client");

    if (Udp.parsePacket())
    {
        // receive incoming UDP packets
        //Serial.printf("Received %d bytes from %s, port %d\n", Udp.remoteIP().toString().c_str(), Udp.remotePort());

        int len = Udp.read(incomingPacket, 255);
        if (len > 0)
        {
            incomingPacket[len] = 0;
        }
        Serial.println("UDP Packet Received");
        Serial.printf("UDP packet contents: %s\n", incomingPacket);

        /*
        // send back a reply, to the IP address and port we got the packet from
        unsigned long startTime = millis();

        Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
        Udp.write(replyPacket);
        Udp.endPacket();  

        unsigned long sendTime = millis()-startTime;
        Serial.print("Sending answer took:");
        Serial.println(sendTime);
    */
        // Serial.print("REQ:");
        // Serial.println(req);

        ///////////////// String editeting///////////////////////
        unsigned long startTime = millis();

        String req(incomingPacket);

        int code = req.charAt(0) - '0';

        switch (code)
        {
        case 0:
        {
            if (req.charAt(2) - '0' == 0)
            {
                //Turn off
                isOn = false;
                // EEPROM.put(0,isOn);
                // EEPROM.commit();
            }
            else if (req.charAt(2) - '0' == 1)
            {
                //Turn on
                isOn = true;
                // EEPROM.put(0,isOn);
                // EEPROM.commit();
                colorsChanged = true;
            }
            else if (req.charAt(2) - '0' == 2)
            {
                Serial.println("\nReboot");
                saveEEPROM();
                ESP.reset();
            }
        }
        break;

        case 1:
        {
            // Get Color
            req = req.substring(2);
            // req Format colorNumber/ColorHex
            int colornumber = req.charAt(0) - '0';
            // remove colurNumber
            req = req.substring(2);
            // Color String to DEC
            long colorLong = strtol(req.c_str(), NULL, 16);

            // Serial.print("long HEX:");
            // Serial.println(colorLong);

            CRGB color = colorLong;

            if (!colorPrevMode)
            {
                colorsOld[colornumber] = colorsNext[colornumber]; //könnte problem geben
            }

            colorsNext[colornumber] = color;

            // if (!colorPrevMode)
            // {
            //     Serial.print("saving color number:");
            //     Serial.print(colornumber);
            //     Serial.print(" set to:");
            //     Serial.println(colorLong);

            //     EEPROM.put(1+colornumber*4,colorLong);
            //     EEPROM.commit();
            // }

            // new Color means Color is not faded
            colorsChanged = true;
        }
        break;

        case 2:
        {
            //Get Animation
            colorsChanged = true;
            int newAnimation = req.substring(2).toInt();

            if (animation != newAnimation)
            {
                iterration = 0;
                currentPos = 0;
                lastTimeAnimation = 0;
            }

            animation = newAnimation;

            // EEPROM.put(13,animation);
            // EEPROM.commit();
        }
        break;

        case 3:
        {
            //Changing globalVariables
            updateVariables = true;
            // colorsChanged = true;
            int globalVariableNumber = req.charAt(2) - '0';
            int globalVariableValue = req.substring(4).toInt();
            globalVariables[globalVariableNumber] = globalVariableValue;

            if (globalVariableNumber == 0)
            {
                //Chaning Brightness
                FastLED.setBrightness(globalVariableValue);
                FastLED.show();
            }

            Serial.print("Changed Variable Number");
            Serial.print(globalVariableNumber);
            Serial.print("   to the Value:");
            Serial.println(globalVariableValue);

            // EEPROM.put(14+globalVariableNumber,globalVariableValue);
            // EEPROM.commit();
        }
        break;

        case 4:
        {
            if (req.charAt(2) - '0' == 0)
            {
                if (!colorPrevMode)
                {
                    //Prev Mode on
                    colorPrevMode = true;
                    //saving colorsBackup
                    for (byte i = 0; i < 3; i++)
                    {
                        colorsBackup[i] = colorsNext[i];
                    }
                }
            }
            else if (req.charAt(2) - '0' == 1)
            {
                //saving colors colorsBackup
                //Prev Mode off
                colorPrevMode = false;
                for (byte i = 0; i < 3; i++)
                {
                    colorsOld[i] = colorsNext[i];
                }
            }
            else if (req.charAt(2) - '0' == 2)
            {
                //Prev Mode off
                colorPrevMode = false;
                //restoring colorsBackup
                for (byte i = 0; i < 3; i++)
                {
                    colorsNext[i] = colorsBackup[i];
                }
                colorsChanged = true;
            }
        }

        case 8:
        {
            // //Setting each led to a specific color
            // animation = -1;
            // EEPROM.write(1,animation);
            // EEPROM.commit();
            // // remove "8/"
            // req = req.substring(2);

            // for (int i = 1; i < COUNTLED; i++)
            // {
            //     if (i*6 < req.length())
            //     {
            //         //FF00FF
            //         String hexCode = req.substring(i*6,i*6+5);

            //         // Color String to DEC
            //         long colorLong = strtol(hexCode.c_str(),NULL,16);

            //         leds[i-1] = CRGB(colorLong);
            //         leds[i] = CRGB(colorLong);
            //         i++;

            //         // Serial.print("LED Number:");
            //         // Serial.print(i);
            //         // Serial.print("   Changed to Color:");
            //         // Serial.println(hexCode);
            //     }else
            //     {
            //         break;
            //     }
            // }

            // FastLED.show();
        }
        break;

        case 9:
        {
            Serial.println("Restoring Defaults and restarting");
            EEPROM.put(0, 0);
            long color1 = 16711680;
            EEPROM.put(1, color1);
            long color2 = 65280;
            EEPROM.put(5, color2);
            long color3 = 255;
            EEPROM.put(9, color3);
            EEPROM.put(13, 0);
            EEPROM.put(14, DEFAULT_globalBrightness);
            EEPROM.put(15, DEFAULT_globalcolortype);
            EEPROM.put(16, DEFAULT_globallightlength);
            EEPROM.put(17, DEFAULT_globaldecay);
            EEPROM.put(18, DEFAULT_globalTimeDelay);
            EEPROM.put(19, DEFAULT_globalOnlyOne);
            EEPROM.put(20, DEFAULT_globalPcMode);
            EEPROM.put(21, DEFAULT_globalSlwoFading);
            EEPROM.put(22, DEFAULT_globalSlwoFadingTime);
            EEPROM.put(23, DEFAULT_globalTransitionOn);

            EEPROM.commit();

            Serial.println("\nReboot");
            ESP.reset();
        }
        break;

        default:
        {
            Serial.println("Wrong User Input");
            //Do Nothing, wrong Input
        }
        break;
        }

        unsigned long tmpTime = millis() - startTime;
        Serial.print("Analysing UDP Content took:");
        Serial.println(tmpTime);
    }
}

//////////////////////////////////////////////////////////////////////
///////////////////////////// LOOP ///////////////////////////////////
//////////////////////////////////////////////////////////////////////

void loop()
{

    /////////////////////////// LOST WIFI
    if (WiFi.status() != WL_CONNECTED)
    {
        if (millis() - lastTimeWIFI > 1000)
        {
            lastTimeWIFI = millis();
            wificonnect();
        }
    }
    else if (WiFi.status() == WL_CONNECTED)
    {
        ////////////////////////// new Requests
        newReq();
    }

    /////////////////////////// Animation Control
    if (isOn)
    {

        if (millis() - lastTimeAnimation > globalVariables[8])
        {
            lastTimeAnimation = millis();
            //Serial.println("animate");
            switch (animation)
            {
                /*
                /0 = static color
                /1 = half
                /2 = drittel
                /3 = runningRainbow
                /4 = bounce
                /5 = light from middle to edges
                /6 = Stack
                /7 = meteorRain
                /8 = twinkle
                /9 = fire
                */

            case 0:
            {
                changeColorToNext();
            }
            break;

            case 1:
            {
                sethalf();
            }
            break;

            case 2:
            {
                setdrittel();
            }

            break;

            case 3:
            {
                runningRainbow();
            }
            break;

            case 4:
            {

            } //bounce
            // light_forward(colora, colorb);
            // light_backward(colora, colorb);
            break;

            case 5:
            {

            } //light form middle to edges
            // light_frommiddle(colora, colorb);
            break;

            case 6:
            {
                stack();
            }
            // stack(colora, colorb);
            break;

            case 7:
            {
                sleepMode();
            }

            break;

            case 8:
            {
                twinkle();
            }
            // meteorRain();
            break;

            case 9:
            {
                //TODO ampel()
            }
            break;

            case 10:
            {
            }
            // fire(false);
            break;

            default:
            {
                //Serial.println("Do Nothing");
                //Do Nothing
                FastLED.delay(WAITDELAY);
            }

            break;
            }
        }
    }
    else
    {
        //If isOn = false
        FastLED.clear();
        FastLED.delay(WAITDELAY);
    }
    //FastLED.delay(WAITDELAY);
    delay(1);
}

///////////////////////////// ANIMATIONS ///////////////////////////////

//int colortype type 0 none only color 1/ typ 1 fade fromm color1 to 2/ type 2 rainbow
CRGB calcColor(byte ledPos, byte start, byte end, CRGB color1, CRGB color2, byte type)
{
    CRGB color;
    if (ledPos >= 0 && ledPos < COUNTLED)
    {
        if (type == 1)
        {
            color = (color1.fadeToBlackBy(((255 * ledPos / (double)end)))) + (color2.fadeToBlackBy(255 - ((255 * ledPos) / (double)end)));

            // int red = color1.red*(i/(double) COUNTLED)+color2.red*(1-(i/(double)COUNTLED));
            // int green = color1.green*(i/(double)COUNTLED)+color2.green*(1-(i/(double)COUNTLED));
            // int blue = color1.blue*(i/(double)COUNTLED)+color2.blue*(1-(i/(double)COUNTLED));
        }
        else if (type == 2)
        {
            color.setHue((255 / end) * ledPos);
        }
        else
        {
            color = color1;
        }
    }

    return color;
}

///////////////////////////// ANIMATION == 0

void changeColorToNext()
{

    if (iterration <= globalVariables[7] && !colorPrevMode && globalVariables[9] == 1 && globalVariables[1] != 2)
    {
        //Serial.println("colorsToNext");
        // unsigned long StartTime = millis();

        CRGB color1 = colorsOld[0];
        CRGB color2 = colorsNext[0];

        //CRGB color = color1.fadeToBlackBy(((256*globalVariables[7])/iterration)) + color2.fadeToBlackBy(256-((256*globalVariables[7])/iterration));
        int red = color2.red * (iterration / (double)globalVariables[7]) + color1.red * (1 - (iterration / (double)globalVariables[7]));
        int green = color2.green * (iterration / (double)globalVariables[7]) + color1.green * (1 - (iterration / (double)globalVariables[7]));
        int blue = color2.blue * (iterration / (double)globalVariables[7]) + color1.blue * (1 - (iterration / (double)globalVariables[7]));

        CRGB color = CRGB(red, green, blue);

        //Serial.println(iterration);
        iterration++;

        fill_solid(leds, COUNTLED, color);

        FastLED.show();
    }
    else if (globalVariables[1] == 2)
    {
        fill_rainbow(leds, COUNTLED, 0);
        FastLED.show();
    }
    else
    {
        fill_solid(leds, COUNTLED, colorsNext[0]);
        FastLED.show();
    }

    colorsChanged = false;
}

///////////////////////////// ANIMATION == 1
void sethalf()
{

    if (colorsChanged)
    {
        Serial.println("sethalf");
        for (int i = 0; i <= (COUNTLED); i++)
        {
            CRGB color1 = colorsNext[1];
            CRGB color2 = colorsNext[0];

            // int red = color1.red*(i/(double) COUNTLED)+color2.red*(1-(i/(double)COUNTLED));
            // int green = color1.green*(i/(double)COUNTLED)+color2.green*(1-(i/(double)COUNTLED));
            // int blue = color1.blue*(i/(double)COUNTLED)+color2.blue*(1-(i/(double)COUNTLED));

            // CRGB color = CRGB(red,green,blue);

            leds[i] = calcColor(i, 0, COUNTLED, color1, color2, 1);
        }

        FastLED.show();
        colorsChanged = false;
    }
}

///////////////////////////// ANIMATION == 2
void setdrittel()
{

    if (colorsChanged)
    {
        switch (globalVariables[1])
        {
        case 0:
            for (int i = 0; i <= (COUNTLED / 3); i++)
            {
                leds[i] = colorsNext[0];
            }

            for (int i = COUNTLED / 3; i <= (COUNTLED / 3) * 2; i++)
            {
                leds[i] = colorsNext[1];
            }

            for (int i = (COUNTLED / 3) * 2; i <= (COUNTLED); i++)
            {
                leds[i] = colorsNext[2];
            }

            break;

        case 1:
            for (int i = 0; i <= (COUNTLED / 2); i++)
            {
                CRGB color1 = colorsNext[0];
                CRGB color2 = colorsNext[1];

                // CRGB color = color1.fadeToBlackBy((255/(COUNTLED/2))*i) + color2.fadeToBlackBy(255-(255/(COUNTLED/2))*i);

                leds[i] = calcColor(i, 0, COUNTLED / 2, color1, color2, 1);
            }

            for (int i = 0; i < COUNTLED / 2; i++)
            {
                CRGB color2 = colorsNext[1];
                CRGB color3 = colorsNext[2];

                // CRGB color = color2.fadeToBlackBy((255/(COUNTLED/2))*i) + color3.fadeToBlackBy(255-(255/(COUNTLED/2))*i);

                leds[COUNTLED / 2 + i] = calcColor(i, 0, COUNTLED / 2, color2, color3, 1);
            }
            break;

        default:
            break;
        }

        FastLED.show();
        colorsChanged = false;
    }
}

///////////////////////////// ANIMATION == 3

void runningRainbow()
{
    fill_rainbow(leds, COUNTLED, hue);
    FastLED.show();
    hue++;
    if (hue > 255)
    {
        hue = 0;
    }
}

///////////////////////////// ANIMATION == 6

void stack()
{
    if (iterration < COUNTLED)
    {
        // Serial.println(currentPos);

        if (iterration == 0 && currentPos == 0)
        {
            Serial.println("Stack");
            FastLED.clear();
            FastLED.show();
        }

        if (COUNTLED - currentPos > iterration)
        {
            CRGB color1 = colorsNext[0];
            CRGB color2 = colorsNext[1];
            if (currentPos != 0)
            {
                leds[COUNTLED - currentPos] = CRGB(0);
            }
            leds[COUNTLED - 1 - currentPos] = calcColor(iterration, 0, COUNTLED, color1, color2, globalVariables[1]);
            // leds[COUNTLED-1-currentPos] = CRGB(255,0,0);

            FastLED.show();
            currentPos++;
        }
        else
        {
            iterration++;
            currentPos = 0;
        }
    }
    else
    {
        iterration = 0;
    }
}

///////////////////////////// ANIMATION == 7

void sleepMode()
{

    if (colorsChanged)
    {
        Serial.println("sleepMode");
        FastLED.clear();
        CRGB color1 = CRGB(255, 0, 0);
        CRGB color2 = CRGB(255, 153, 0);
        for (byte i = 0; i < 20; i++)
        {
            leds[i] = calcColor(i, 0, 20, color1, color2, 1).fadeToBlackBy(200);
        }

        FastLED.show();
        colorsChanged = false;
    }
}

///////////////////////////// ANIMATION == 8

//color 1 = leds[0] / color2 = [max] / doFade fade from color1 to 2 / onlyOne led on at the time / reset strip after one run
//int colortype type 0 none only color 1/ typ 1 fade fromm color1 to 2/ type 2 rainbow
void twinkle()
{
    CRGB color1 = colorsNext[0];
    CRGB color2 = colorsNext[1];

    byte position = random(COUNTLED);
    leds[position] = calcColor(position, 0, COUNTLED, color1, color2, globalVariables[1]);
    for (int j = 0; j < 10; j++)
        leds[random(COUNTLED)].fadeToBlackBy((globalVariables[3]));
    FastLED.show();
}