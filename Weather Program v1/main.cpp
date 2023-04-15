#include <M5Core2.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "EGR425_Phase1_weather_bitmap_images.h"
#include "WiFi.h"

////////////////////////////////////////////////////////////////////
// Variables
////////////////////////////////////////////////////////////////////
// Register for openweather account and get API key
String urlOpenWeather = "https://api.openweathermap.org/data/2.5/weather?";
String apiKey = "065402ddae46310356b8359f16433c33";

// WiFi variables
String wifiNetworkName = "CBU-LancerArmsWest";
String wifiPassword = "";


// Time variables
unsigned long lastTime = 0;
unsigned long timerDelay = 5000; // 5000; 5 minutes (300,000ms) or 5 seconds (5,000ms)

// LCD variables
int sWidth;
int sHeight;

// Weather/zip variables
String strWeatherIcon;
String strWeatherDesc;
String cityName;
double tempNow;
double tempMin;
double tempMax;

// mode variable
enum Unit
{
    U_FAHRENHEIT,
    U_CELSIUS
};

enum Screen
{
    S_LOADING,
    S_WEATHER,
    S_ZIP
};

static Screen screen;
static Unit unit;

// timestamp variable
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "us.pool.ntp.org");
int hours;
int minutes;
int seconds;
String amPm;

// zipcode variables
int zipcode[5] = {9, 2, 5, 0, 4};
const int NUM_ZIPCODE_DIGITS = 5;

// zipcode button variables
const int NUM_ZIPCODE_BUTTONS = NUM_ZIPCODE_DIGITS * 2;
Button buttons[NUM_ZIPCODE_BUTTONS] = {Button(0, 0, 0, 0, ""), Button(0, 0, 0, 0, ""), Button(0, 0, 0, 0, ""),
                                       Button(0, 0, 0, 0, ""), Button(0, 0, 0, 0, ""), Button(0, 0, 0, 0, ""),
                                       Button(0, 0, 0, 0, ""), Button(0, 0, 0, 0, ""), Button(0, 0, 0, 0, ""),
                                       Button(0, 0, 0, 0, "")};

////////////////////////////////////////////////////////////////////
// Method header declarations
////////////////////////////////////////////////////////////////////
String httpGETRequest(const char *serverName);
void timeStampProcessing();
double toFahrenheit(double kelvinTemp);
double toCelcius(double kelvinTemp);
void drawWeatherImage(String iconId, int resizeMult);
void fetchWeatherDetails();
void drawWeatherDisplay();
void drawZipCodeDisplay();
void drawFetchingDisplay();
int splitName(String name);

///////////////////////////////////////////////////////////////
// Put your setup code here, to run once
///////////////////////////////////////////////////////////////
void setup()
{
    // Initialize the device
    M5.begin();

    // Set screen orientation and get height/width
    sWidth = M5.Lcd.width();
    sHeight = M5.Lcd.height();

    // set the states
    screen = S_LOADING;
    unit = U_FAHRENHEIT;

    drawFetchingDisplay();

    // set default weather values
    strWeatherIcon = "";
    strWeatherDesc = "";
    cityName = "";
    tempMin = 0.0;
    tempMax = 0.0;
    tempNow = 0.0;

    // Connect to WiFi
    WiFi.begin(wifiNetworkName.c_str(), wifiPassword.c_str());
    Serial.printf("Connecting");
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }
    Serial.print("\n\nConnected to WiFi network with IP address: ");
    Serial.println(WiFi.localIP());

    // timestamp
    timeClient.begin();
}

///////////////////////////////////////////////////////////////
// Put your main code here, to run repeatedly
///////////////////////////////////////////////////////////////
void loop()
{
    M5.update();

    // Only execute every so often
    if ((millis() - lastTime) > timerDelay)
    {
        if (WiFi.status() == WL_CONNECTED)
        {

            // timestamp updating
            timeStampProcessing();

            // get weather details, and update the screen
            fetchWeatherDetails();
            if (screen == S_WEATHER)
            {
                drawWeatherDisplay();
            }
        }
        else
        {
            Serial.println("WiFi Disconnected");
        }

        // Update the last time to NOW
        lastTime = millis();
    }

    if (screen == S_LOADING)
    {
        return;
    } 
    // anything below here will not be executed if loading

    // M5 buttons
    if (M5.BtnA.wasPressed())
    {
        // change the unit state
        unit = unit == U_FAHRENHEIT ? U_CELSIUS : U_FAHRENHEIT;

        // update the screen with the new units
        drawWeatherDisplay();
    }

    if (M5.BtnB.wasPressed())
    {
        if (screen == S_WEATHER)
        {
            screen = S_ZIP;
            drawZipCodeDisplay();
        }
        else if (screen == S_ZIP)
        {
            drawFetchingDisplay();
            screen = S_LOADING;
            fetchWeatherDetails();
        }
    }

    // Touch Buttons
    for (int i = 0; i < NUM_ZIPCODE_BUTTONS; i++)
    {
        if (buttons[i].wasPressed())
        {
            if (i < NUM_ZIPCODE_DIGITS)
            {
                zipcode[i] = (zipcode[i] + 1) % 10;
            }
            else
            {
                zipcode[i % NUM_ZIPCODE_DIGITS] = (zipcode[i % NUM_ZIPCODE_DIGITS] - 1) % NUM_ZIPCODE_BUTTONS;
                if (zipcode[i % NUM_ZIPCODE_DIGITS] < 0)
                {
                    zipcode[i % NUM_ZIPCODE_DIGITS] = NUM_ZIPCODE_BUTTONS - 1;
                }
            }
            drawZipCodeDisplay();
        }
    }
}

/////////////////////////////////////////////////////////////////
// Calculates the current timestamp and updates it accordingly
/////////////////////////////////////////////////////////////////
void timeStampProcessing()
{
    timeClient.update();
    hours = (timeClient.getHours() + 16) % 24;
    minutes = timeClient.getMinutes();
    seconds = timeClient.getSeconds();

    // am/pm processing
    if (hours > 12)
    { // PM in 24-hour clock
        amPm = "PM";
        hours -= 12;
    }
    else
    {
        if (hours == 0)
        {
            hours = 12; // midnight
        }
        amPm = "AM";
    }
}

/////////////////////////////////////////////////////////////////
// Converts the temperature from Kelvin to F
/////////////////////////////////////////////////////////////////
double toFahrenheit(double kelvinTemp)
{
    double fahrenheitTemp = 1.8 * (kelvinTemp - 273.0) + 32.0;
    return fahrenheitTemp;
}

/////////////////////////////////////////////////////////////////
// Converts the temperature from Kelvin to  C
/////////////////////////////////////////////////////////////////
double toCelcius(double kelvinTemp)
{
    double celciusTemp = kelvinTemp - 273.15;
    return celciusTemp;
}

/////////////////////////////////////////////////////////////////
// This method fetches the weather details from the OpenWeather
// API and saves them into the fields defined above
/////////////////////////////////////////////////////////////////
void fetchWeatherDetails()
{
    String zipcodeStr = "";
    for (int i = 0; i < NUM_ZIPCODE_DIGITS; i++)
    {
        zipcodeStr += String(zipcode[i]);
    }

    String serverURL = urlOpenWeather + "zip=" + zipcodeStr + ",us&appid=" + apiKey;

    // Serial.println(serverURL); // Debug print

    //////////////////////////////////////////////////////////////////
    // Make GET request and store reponse
    //////////////////////////////////////////////////////////////////
    String response = httpGETRequest(serverURL.c_str());
    Serial.print(response); // Debug print

    //////////////////////////////////////////////////////////////////
    // Import ArduinoJSON Library and then use arduinojson.org/v6/assistant to
    // compute the proper capacity (this is a weird library thing) and initialize
    // the json object
    //////////////////////////////////////////////////////////////////
    const size_t jsonCapacity = 768 + 250;
    DynamicJsonDocument objResponse(jsonCapacity);

    if (objResponse["cod"] == "404")
    {
        Serial.print("404 ERROR");
        if (screen == S_LOADING)
        {
            drawZipCodeDisplay();
            screen = S_ZIP;
        }
        return;
    }

    //////////////////////////////////////////////////////////////////
    // Deserialize the JSON document and test if parsing succeeded
    //////////////////////////////////////////////////////////////////
    DeserializationError error = deserializeJson(objResponse, response);
    if (error)
    {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        return;
    }
    // serializeJsonPretty(objResponse, Serial); // Debug print

    //////////////////////////////////////////////////////////////////
    // Parse Response to get the weather description and icon
    //////////////////////////////////////////////////////////////////
    JsonArray arrWeather = objResponse["weather"];
    JsonObject objWeather0 = arrWeather[0];
    String desc = objWeather0["main"];
    String icon = objWeather0["icon"];
    String city = objResponse["name"];

    // ArduionJson library will not let us save directly to these
    // variables in the 3 lines above for unknown reason
    strWeatherDesc = desc;
    strWeatherIcon = icon;
    cityName = city;

    // Parse response to get the temperatures
    JsonObject objMain = objResponse["main"];
    tempNow = objMain["temp"];
    tempMin = objMain["temp_min"];
    tempMax = objMain["temp_max"];
    // Serial.printf("NOW: %.1f F and %s\tMIN: %.1f F\tMax: %.1f F\n", tempNow, strWeatherDesc, tempMin, tempMax);

    if (screen == S_LOADING)
    {
        drawWeatherDisplay();
        lastTime = millis();
        screen = S_WEATHER;
    }
}

/////////////////////////////////////////////////////////////////
// Update the display based on the weather variables defined
// at the top of the screen.
/////////////////////////////////////////////////////////////////
void drawWeatherDisplay()
{
    //////////////////////////////////////////////////////////////////
    // Draw background - light blue if day time and navy blue of night
    //////////////////////////////////////////////////////////////////
    uint16_t primaryTextColor;
    if (strWeatherIcon.indexOf("d") >= 0)
    {
        M5.Lcd.fillScreen(TFT_CYAN);
        primaryTextColor = TFT_BLACK;
    }
    else
    {
        M5.Lcd.fillScreen(TFT_NAVY);
        primaryTextColor = TFT_LIGHTGREY;
    }

    //////////////////////////////////////////////////////////////////
    // Draw the icon on the right side of the screen - the built in
    // drawBitmap method works, but we cannot scale up the image
    // size well, so we'll call our own method
    //////////////////////////////////////////////////////////////////
    // M5.Lcd.drawBitmap(0, 0, 100, 100, myBitmap, TFT_BLACK);
    drawWeatherImage(strWeatherIcon, 2);

    //////////////////////////////////////////////////////////////////
    // Draw the temperatures and city name
    //////////////////////////////////////////////////////////////////
    int pad = 10;
    // displaying variables
    char units = 'F';
    double tMin = tempMin;
    double tMax = tempMax;
    double tNow = tempNow;

    if (unit == U_FAHRENHEIT)
    {
        units = 'F';
        tMin = toFahrenheit(tempMin);
        tMax = toFahrenheit(tempMax);
        tNow = toFahrenheit(tempNow);
    }
    else if (unit == U_CELSIUS)
    {
        units = 'C';
        tMin = toCelcius(tempMin);
        tMax = toCelcius(tempMax);
        tNow = toCelcius(tempNow);
    }

    M5.Lcd.setCursor(pad, pad);
    if (strWeatherIcon.indexOf("d") >= 0)
    {
        M5.Lcd.setTextColor(TFT_BLUE);
    }
    else
    {
        M5.Lcd.setTextColor(TFT_CYAN);
    }

    M5.Lcd.setTextSize(3);
    M5.Lcd.printf("LO:%0.f%c\n", tMin, units);

    M5.Lcd.setCursor(pad, M5.Lcd.getCursorY());
    if (strWeatherIcon.indexOf("d") >= 0)
    {
        M5.Lcd.setTextColor(TFT_DARKGREY);
    }
    else
    {
        M5.Lcd.setTextColor(TFT_YELLOW);
    }
    M5.Lcd.setTextSize(10);
    M5.Lcd.printf("%0.f%c\n", tNow, units);

    M5.Lcd.setCursor(pad, M5.Lcd.getCursorY());
    M5.Lcd.setTextColor(TFT_RED);
    M5.Lcd.setTextSize(3);
    M5.Lcd.printf("HI:%0.f%c\n", tMax, units);

    M5.Lcd.setCursor(pad, M5.Lcd.getCursorY());
    M5.Lcd.setTextColor(primaryTextColor);

    String tempName = cityName;
    int spaceIndex;
    while (tempName.length() >= 15)
    {
        spaceIndex = splitName(tempName);
        M5.Lcd.printf("%s\n", tempName.substring(0, spaceIndex).c_str());
        M5.Lcd.setCursor(pad, M5.Lcd.getCursorY());

        tempName = tempName.substring(spaceIndex + 1);
    }

    M5.Lcd.printf("%s\n", tempName.c_str());

    M5.Lcd.setCursor(pad, M5.Lcd.getCursorY());
    M5.Lcd.setTextColor(primaryTextColor);
    M5.Lcd.setTextSize(2);
    M5.Lcd.printf("Last updated:\n %02d:%02d:%02d %s\n", hours, minutes, seconds, amPm);
}

int splitName(String name)
{
    String temp = name.substring(0, 15);
    return temp.lastIndexOf(" ");
}

/////////////////////////////////////////////////////////////////
// Update the display to set a zip code
/////////////////////////////////////////////////////////////////
void drawZipCodeDisplay()
{
    // background and text color
    uint16_t primaryTextColor;
    M5.Lcd.fillScreen(TFT_BLACK);
    primaryTextColor = TFT_WHITE;

    // variables
    int pad = 10;
    int xOffset = sWidth / 11;
    int halfXOffset = xOffset / 2;
    // int yOffset = 40;
    int triHeight = 20;
    int triWidth = sWidth / 11;
    int vSpace = 10;

    // title
    M5.Lcd.setCursor(pad, pad);
    M5.Lcd.setTextColor(primaryTextColor);
    M5.Lcd.setTextSize(2);
    M5.Lcd.println("Enter a zip code: ");

    // draw buttons and numbers
    for (int i = 0; i < NUM_ZIPCODE_DIGITS; i++)
    {
        xOffset = ((i * 2) + 1) * (sWidth / 11);
        int yOffset = 40; // offset for the text

        // draw the up arrow
        M5.Lcd.fillTriangle(xOffset, 2 * yOffset, triWidth + xOffset, 2 * yOffset, triWidth / 2 + xOffset, yOffset + triHeight, primaryTextColor);

        // add the touchzone
        buttons[i] = Button(xOffset, yOffset + triHeight, triWidth, triHeight + 20, ("up %d", i));
        Button btn = buttons[i];
        // M5.Lcd.fillRect(btn.x, btn.y, btn.w, btn.h, RED);

        yOffset = (yOffset * 2) + triHeight + vSpace;

        // draw the digit
        M5.Lcd.setTextColor(primaryTextColor);
        M5.Lcd.drawString(String(zipcode[i]), xOffset, yOffset, 4);
        yOffset = yOffset + 28 + (vSpace);

        // draw the down arrow
        M5.Lcd.fillTriangle(xOffset, yOffset + 40, triWidth + xOffset, yOffset + 40, triWidth / 2 + xOffset, yOffset + (triHeight * 3), primaryTextColor);

        // add the touchzone
        buttons[i + NUM_ZIPCODE_DIGITS] = Button(xOffset, (yOffset + (triHeight * 2) - 20), triWidth, triHeight + 20, ("down %d", i + NUM_ZIPCODE_DIGITS));
        Button btn1 = buttons[i + NUM_ZIPCODE_DIGITS];
        // M5.Lcd.fillRect(btn1.x, btn1.y, btn1.w, btn1.h, GREEN);
    }
}

void drawFetchingDisplay()
{
    M5.Lcd.fillScreen(TFT_BLACK);
    M5.Lcd.setTextColor(TFT_WHITE);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(35, sHeight / 2 - 10);
    M5.Lcd.print("Fetching Weather Data");
}

/////////////////////////////////////////////////////////////////
// This method takes in a URL and makes a GET request to the
// URL, returning the response.
/////////////////////////////////////////////////////////////////
String httpGETRequest(const char *serverURL)
{

    // Initialize client
    HTTPClient http;
    http.begin(serverURL);

    // Send HTTP GET request and obtain response
    int httpResponseCode = http.GET();
    String response = http.getString();

    // Check if got an error
    if (httpResponseCode > 0)
        Serial.printf("HTTP Response code: %d\n", httpResponseCode);
    else
    {
        Serial.printf("HTTP Response ERROR code: %d\n", httpResponseCode);
        Serial.printf("Server Response: %s\n", response);
    }

    // Free resources and return response
    http.end();
    return response;
}

/////////////////////////////////////////////////////////////////
// This method takes in an image icon string (from API) and a
// resize multiple and draws the corresponding image (bitmap byte
// arrays found in EGR425_Phase1_weather_bitmap_images.h) to scale (for
// example, if resizeMult==2, will draw the image as 200x200 instead
// of the native 100x100 pixels) on the right-hand side of the
// screen (centered vertically).
/////////////////////////////////////////////////////////////////
void drawWeatherImage(String iconId, int resizeMult)
{

    // Get the corresponding byte array
    const uint16_t *weatherBitmap = getWeatherBitmap(iconId);

    // Compute offsets so that the image is centered vertically and is
    // right-aligned
    int yOffset = (-(resizeMult * imgSqDim - M5.Lcd.height()) / 2) - 60;
    int xOffset = sWidth - (imgSqDim * resizeMult * .8) - 20; // Right align (image doesn't take up entire array)
    // int xOffset = (M5.Lcd.width() / 2) - (imgSqDim * resizeMult / 2); // center horizontally

    // Iterate through each pixel of the imgSqDim x imgSqDim (100 x 100) array
    for (int y = 0; y < imgSqDim; y++)
    {
        for (int x = 0; x < imgSqDim; x++)
        {
            // Compute the linear index in the array and get pixel value
            int pixNum = (y * imgSqDim) + x;
            uint16_t pixel = weatherBitmap[pixNum];

            // If the pixel is black, do NOT draw (treat it as transparent);
            // otherwise, draw the value
            if (pixel != 0)
            {
                // 16-bit RBG565 values give the high 5 pixels to red, the middle
                // 6 pixels to green and the low 5 pixels to blue as described
                // here: http://www.barth-dev.de/online/rgb565-color-picker/
                byte red = (pixel >> 11) & 0b0000000000011111;
                red = red << 3;
                byte green = (pixel >> 5) & 0b0000000000111111;
                green = green << 2;
                byte blue = pixel & 0b0000000000011111;
                blue = blue << 3;

                // Scale image; for example, if resizeMult == 2, draw a 2x2
                // filled square for each original pixel
                for (int i = 0; i < resizeMult; i++)
                {
                    for (int j = 0; j < resizeMult; j++)
                    {
                        int xDraw = x * resizeMult + i + xOffset;
                        int yDraw = y * resizeMult + j + yOffset;
                        M5.Lcd.drawPixel(xDraw, yDraw, M5.Lcd.color565(red, green, blue));
                    }
                }
            }
        }
    }
}