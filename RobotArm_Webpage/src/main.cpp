#include <Arduino.h>
#include <PrintStream.h>
#include "taskshare.h"
#include "taskqueue.h"
#include "shares.h"

#include <ESP32Servo.h>
#include "task_flx.h"
#include "task_fingers.h"
#include <WiFi.h>
#include <WebServer.h>

// #define USE_LAN to have the ESP32 join an existing Local Area Network or 
// #undef USE_LAN to have the ESP32 act as an access point, forming its own LAN
#undef USE_LAN

// If joining an existing LAN, get certifications from a header file which you
// should NOT push to a public repository of any kind
#ifdef USE_LAN
#include "mycerts.h"       // For access to your WiFi network; see setup_wifi()

// If the ESP32 creates its own access point, put the credentials and network
// parameters here; do not use any personally identifying or sensitive data
#else
const char* ssid = "robot_arm";   // SSID, network name seen on LAN lists
const char* password = "pr0jp@$$";   // ESP32 WiFi password (min. 8 characters)

/* Put IP Address details */
IPAddress local_ip (192, 168, 5, 1); // Address of ESP32 on its own network
IPAddress gateway (192, 168, 5, 1);  // The ESP32 acts as its own gateway
IPAddress subnet (255, 255, 255, 0); // Network mask; just leave this as is
#endif


/// The pin connected to an LED controlled through the Web interface
// const uint8_t ledPin = 2;

// #define FAST_PIN 12         ///< The GPIO pin cranking out a 500 Hz square wave


/** @brief   The web server object for this project.
 *  @details This server is responsible for responding to HTTP requests from
 *           other computers, replying with useful information.
 *
 *           It's kind of clumsy to have this object as a global, but that's
 *           the way Arduino keeps things simple to program, without the user
 *           having to write custom classes or other intermediate-level 
 *           structures. 
*/
WebServer server (80);

/** @brief   Get the WiFi running so we can serve some web pages.
 */
void setup_wifi (void)
{
#ifdef USE_LAN                           // If connecting to an existing LAN
    Serial << "Connecting to " << ssid;

    // The SSID and password should be kept secret in @c mycerts.h.
    // This file should contain the two lines,
    //   const char* ssid = "YourWiFiNetworkName";
    //   const char* password = "YourWiFiPassword";
    WiFi.begin (ssid, password);

    while (WiFi.status() != WL_CONNECTED) 
    {
        vTaskDelay (1000);
        Serial.print (".");
    }

    Serial << "connected at IP address " << WiFi.localIP () << endl;

#else                                   // If the ESP32 makes its own LAN
    Serial << "Setting up WiFi access point...";
    WiFi.mode (WIFI_AP);
    WiFi.softAPConfig (local_ip, gateway, subnet);
    WiFi.softAP (ssid, password);
    Serial << "done." << endl;
#endif
}


/** @brief   Put a web page header into an HTML string. 
 *  @details This header may be modified if the developer wants some actual
 *           @a style for her or his web page. It is intended to be a common
 *           header (and stylle) for each of the pages served by this server.
 *  @param   a_string A reference to a string to which the header is added; the
 *           string must have been created in each function that calls this one
 *  @param   page_title The title of the page
*/
void HTML_header (String& a_string, const char* page_title)
{
    a_string += "<!DOCTYPE html> <html>\n";
    a_string += "<head><meta name=\"viewport\" content=\"width=device-width,";
    a_string += " initial-scale=1.0, user-scalable=no\">\n<title> ";
    a_string += page_title;
    a_string += "</title>\n";
    a_string += "<style>html { font-family: Helvetica; display: inline-block;";
    a_string += " margin: 0px auto; text-align: center;}\n";
    a_string += "body{margin-top: 50px;} h1 {color: #4444AA;margin: 50px auto 30px;}\n";
    a_string += "p {font-size: 24px;color: #222222;margin-bottom: 10px;}\n";
    a_string += "</style>\n</head>\n";
}


/** @brief   Callback function that responds to HTTP requests without a subpage
 *           name.
 *  @details When another computer contacts this ESP32 through TCP/IP port 80
 *           (the insecure Web port) with a request for the main web page, this
 *           callback function is run. It sends the main web page's text to the
 *           requesting machine.
 */
void handle_DocumentRoot ()
{
    Serial << "HTTP request from client #" << server.client () << endl;

    String a_str;
    HTML_header (a_str, "ESP32 Web Server Test");
    a_str += "<body>\n<div id=\"webpage\">\n";
    a_str += "<h1>Test Main Page</h1>\n";
    a_str += "...or is it Main Test Page?\n";
    a_str += "<p><p> <a href=\"/toggle\">Toggle Pinky</a>\n";
    // a_str += "<p><p> <a href=\"/csv\">Show some data in CSV format</a>\n";
    a_str += "<p><p> <a href=\"/csv\">Flex sensor data</a>\n";
    a_str += "</div>\n</body>\n</html>\n";

    server.send (200, "text/html", a_str); 
}


/** @brief   Respond to a request for an HTTP page that doesn't exist.
 *  @details This function produces the Error 404, Page Not Found error. 
 */
void handle_NotFound (void)
{
    server.send (404, "text/plain", "Not found");
}


/** @brief   Toggle blue LED when called by the web server.
 *  @details For testing purposes, this function turns the little blue LED on a
 *           38-pin ESP32 board on and off. It is called when someone enters
 *           @c http://server.address/toggle as the web address request from a
 *           browser.
 */
void handle_Toggle_pinky (void)
{
    // This variable must be declared static so that its value isn't forgotten
    // each time this function runs. BUG: It takes two requests to the toggle
    // page before the LED turns on, after which it toggles as expected.
    static bool state = false;
       if (state = true)
       {
        pinky_pwm.put(180);
       }
       else
       {
        pinky_pwm.put(0);
       }
    state = !state;

    String toggle_page = "<!DOCTYPE html> <html> <head>\n";
    toggle_page += "<meta http-equiv=\"refresh\" content=\"1; url='/'\" />\n";
    toggle_page += "</head> <body> <p> <a href='/'>Back to main page</a></p>";
    toggle_page += "</body> </html>";

    server.send (200, "text/html", toggle_page); 
}

void handle_flx (void)
{
    // The page will be composed in an Arduino String object, then sent.
    // The first line will be column headers so we know what the data is
    String flx_str = "Thumb, Pointer, Middle, Ring, Pinky\n";

    // Create some fake data and put it into a String object. We could just
    // as easily have taken values from a data array, if such an array existed
    flx_str += String (thumb_pwm.get());
    flx_str += ",";
    flx_str += String (pointer_pwm.get());
    flx_str += ",";
    flx_str += String (middle_pwm.get());
    flx_str += ",";
    flx_str += String (ring_pwm.get());
    flx_str += ",";
    flx_str += String (pinky_pwm.get());
    flx_str += "\n";

    // Send the CSV file as plain text so it can be easily saved as a file
    server.send (404, "text/plain", flx_str);
}

// /** @brief   Show some simulated data when asked by the web server.
//  *  @details The contrived data is sent in a relatively efficient Comma
//  *           Separated Variable (CSV) format which is easily read by Matlab(tm)
//  *           and Python and spreadsheets.
//  */
// void handle_CSV (void)
// {
//     // The page will be composed in an Arduino String object, then sent.
//     // The first line will be column headers so we know what the data is
//     String csv_str = "Time, Jumpiness\n";

//     // Create some fake data and put it into a String object. We could just
//     // as easily have taken values from a data array, if such an array existed
//     for (uint8_t index = 0; index < 20; index++)
//     {
//         csv_str += index;
//         csv_str += ",";
//         csv_str += String (sin (index / 5.4321), 3);       // 3 decimal places
//         csv_str += "\n";
//     }
//
//     // Send the CSV file as plain text so it can be easily saved as a file
//     server.send (404, "text/plain", csv_str);
// }


/** @brief   Task which sets up and runs a web server.
 *  @details After setup, function @c handleClient() must be run periodically
 *           to check for page requests from web clients. One could run this
 *           task as the lowest priority task with a short or no delay, as there
 *           generally isn't much rush in replying to web queries.
 *  @param   p_params Pointer to unused parameters
 */
void task_webserver (void* p_params)
{
    // The server has been created statically when the program was started and
    // is accessed as a global object because not only this function but also
    // the page handling functions referenced below need access to the server
    server.on ("/", handle_DocumentRoot);
    server.on ("/toggle", handle_Toggle_pinky);
    // server.on ("/csv", handle_CSV);
    server.on ("/csv", handle_flx);
    server.onNotFound (handle_NotFound);

    // Get the web server running
    server.begin ();
    Serial.println ("HTTP server started");

    for (;;)
    {
        // The web server must be periodically run to watch for page requests
        server.handleClient ();
        vTaskDelay (500);
    }
}

void task_print(void* p_params)
{
  for(;;)
  {
    Serial.print(thumb_pwm.get());
    Serial.print(',');
    Serial.print(pointer_pwm.get());
    Serial.print(',');
    Serial.print(middle_pwm.get());
    Serial.print(',');
    Serial.print(ring_pwm.get());
    Serial.print(',');
    Serial.println(pinky_pwm.get());
    vTaskDelay (1000);
  }
}

void setup(void) {
  Serial.begin(115200);
  while (!Serial)
    delay(10); // will pause Zero, Leonardo, etc until serial console opens

  Serial.println("");
  delay(100);

  // Call function which gets the WiFi working
  setup_wifi ();

  // Task which runs the web server. It runs at a low priority
  xTaskCreate (task_webserver, "Web Server", 8192, NULL, 2, NULL);

  xTaskCreate (task_flx, "Flex Sensor", 2048, NULL, 4, NULL);
  xTaskCreate (task_fingers, "Finger Servos", 2048, NULL, 5, NULL);
  xTaskCreate (task_print, "Print PWM", 2048, NULL, 1, NULL);

}

void loop (void) {}