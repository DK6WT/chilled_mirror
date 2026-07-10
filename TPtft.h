/*
 * TP-3000 Taupunktspiegel-Hygrometer
 * Datei: TPtft.h
 * Zweck: Deklarationen der RA8875-Grafik- und TextBox-Hilfsklassen.
 *
 * Abgeleitet aus: PSWRtft.h
 * aus LJ2000M T_2.06c GSL1680.
 *   Copyright (C) 2016 Loftur E. Jonasson
 *   Copyright (C) 2017-2021 J.G. Holstein
 * TP-3000-Umbau und weitere Aenderungen:
 *   Copyright (C) 2025-2026 S. Brachtl (DK6WT)
 *
 * SPDX-License-Identifier: GPL-3.0-only
 * Lizenz- und Herkunftsdetails: README.md und THIRD_PARTY_NOTICES.md
 */

#define LTBLUE          0xB6DF
#define LTTEAL          0xBF5F
#define LTGREEN         0xBFF7
#define LTCYAN          0xC7FF
#define LTRED           0xFD34
#define LTMAGENTA       0xFD5F
#define LTYELLOW        0xFFF8
#define LTORANGE        0xFE73
#define LTPINK          0xFDDF
#define LTPURPLE        0xCCFF
#define LTGREY          0xE71C
#define BLUE            0x001F
#define TEAL            0x0438
#define GREEN           0x07E0
#define CYAN            0x07FF
#define RED             0xF800
#define MAGENTA         0xF81F
#define YELLOW          0xFFE0
#define ORANGE          0xFD20
#define PINK            0xF81F
#define PURPLE          0x801F
#define GREY            0xC618
#define WHITE           0xFFFF
#define BLACK           0x0000
#define DKBLUE          0x000D
#define DKTEAL          0x020C
#define DKGREEN         0x03E0
#define DKCYAN          0x03EF
#define DKRED           0x6000
#define DKMAGENTA       0x8008
#define DKYELLOW        0x8400
#define DKORANGE        0x8200
#define DKPINK          0x9009
#define DKPURPLE        0x4010
#define DKGREY          0x4A49
#define DKDKGREY        0x10A3
#define BACKCOLOR BLACK
#define LTBACKCOLOR WHITE
#define BORDERCOLOR BLACK
#ifndef _TPtft_h_
#define _TPtft_h_
#include <Arduino.h>
#include <SPI.h>
#include <RA8875.h>
#include "fonts.h"
class PowerMeter
{
  public:
    //------------------------------------------------------------------------------
    // Init size, shape and colour of graph
    //           left x;    top y; length x; height y;   graph, low and high colour 
    void init (int16_t, int16_t, int16_t, int16_t, int16_t, int16_t, int16_t);
    void init (int16_t, int16_t, int16_t, int16_t);
    //------------------------------------------------------------------------------
    // Draw, Redraw or Erase Meter Scale.  range provides optinal unit information, such as "mW", "W", "kW"
    // input argument is a number from 1.0 and up, divided into 10  or 11 segments (11 if (scale*10)%11=0 )
    void scale(double s, char *range);      // Scale Erases if required and then draws everything
    void scale(double s) { scale(s, (char*)""); }// Blank if no scale range string
    void erase(void);                       // Erases everything
    //------------------------------------------------------------------------------
    // Draw Meter Graph
    // input arguments are low value, high value and full scale value
    void graph(double, double, double);

  private:
    void drawscale(double, int16_t, char*); // Draws the scale for the graph. Colour to draw/erase
    void printunits(int16_t, char*);        // Prints an units indication at the last scale tick
    void drawframe(int16_t);                // Draws the frame around the graph
    void erasegraph(void);                  // Erase the current graph contents
    int16_t x           = 1; 
    int16_t y           = 15;
    int16_t len         = 725;
    int16_t height      = 70;
    int16_t GaugeColour = GREY;
    int16_t LowColour   = YELLOW;
    int16_t HighColour  = BLUE; 
    int16_t blnkColour  = BLACK;
    double  lastmax;                        // Max Level for full readout.  We have to redraw all if this changes
    int16_t lastlow, lasthigh;              // Prev levels for the two gauges, used to redraw only what is necessary
    double  current_scale = 0;
    char    current_range[10];
    bool    erased = true;                  // Keep track of whether graph needs to be redrawn
};


class VSWRmeter
{
 
  public:
    
    //------------------------------------------------------------------------------
    // Init size, shape and colour of VSWR graph
    //        left x; top y; length x; height y;  graph, low SWR,  mid SWR, high SWR colour 
    void init (int16_t, int16_t, int16_t, int16_t, int16_t, int16_t, int16_t, int16_t);
    void init (int16_t, int16_t, int16_t, int16_t);
    //------------------------------------------------------------------------------
    // Draw or Erase VSWR Meter Scale (no value)
    void scale(void);
    void erase(void);  // Erases everything
    //------------------------------------------------------------------------------
    // Draw VSWR Meter Graph
    // input arguments are mid SWR, high SWR and actual SWR input value
    void graph(double, double, double);

  private:
    void drawscale(int16_t);
    int16_t x = 1; 
    int16_t y = 175;
    int16_t len         = 725; //300
    int16_t height      = 70;
    int16_t GaugeColour = GREY;
    int16_t lSWRcolour  = GREEN;
    int16_t mSWRcolour  = ORANGE;
    int16_t hSWRcolour  = RED;
    int16_t blnkColour  = BLACK;
    int16_t scalemid, scalehigh;        // Calculated log10 of mid and high values
    int16_t lastlow, lastmid, lasthigh; // Previous values, used to redraw only what is necessary
    double  midthresh, highthresh;      // Keep track of Mid and High thresholds
    bool    erased = true;              // Keep track of whether graph needs to be redrawn
};


#define SCOPE_BUFSIZE  781            // NOTE: Max number of iterations during one X_trace iteration

class ModulationScope
{
  public:
    //------------------------------------------------------------------------------
    // Init size, shape and colour of Modulation Scope
    //          left x;   top y; length x; height y; border, trace and peak colour 
    void init (int16_t, int16_t, int16_t, int16_t, int16_t, int16_t, int16_t);
    void init (int16_t, int16_t, int16_t, int16_t);
    void init (void) { init(3, 150, 781, 190); }  // Default x, y coordinate settings

    //------------------------------------------------------------------------------
    // Draw scope if need be, and draw pending data onscreen
    void update(void);
    //------------------------------------------------------------------------------
    // Erase the scope widget
    void erase(void);
    //------------------------------------------------------------------------------
    // Update rate divisor
    void rate(int16_t);
    //------------------------------------------------------------------------------
    // Add one measurement to the x axis sweep (run from within interrupt function)
    // input arguments are level and fullscale
    void adddata(double, double);

  private:
    void draw(void);                // Draw scope if needed
    int16_t x;        //=   5; 
    int16_t y ;       //=   5;
    int16_t len;     //= 310;
    int16_t height;   //= 180;
    int16_t borderColour= GREY;
    int16_t traceColour = GREEN;
    int16_t peakColour  = YELLOW;
    int16_t blnkColour  = BLACK;
    int16_t y_axis0;                // Start positions and y axis max deviation, derived from x,y, len, height
    int16_t x_axis0;
    int16_t y_max;
    int16_t newdata[SCOPE_BUFSIZE]; // New data for the scope - fed with data from a circular buffer fed by the interrupt funciton
    int16_t in_pos;                 // Two circular buffer pointers
    int16_t out_pos;                // this one is also current position on the x axis
    int16_t olddata[SCOPE_BUFSIZE]; // Current data on the scope - to add or subtract from...
    int16_t rateDivisor=1;          // Rate divisor other than 1 results in only every n_th request drawn
    int16_t rateCount;
    bool    erased = false;         // Keep track of whether scope needs to be erased
    bool    drawn  = false;         // Keep track of whether scope needs to be redrawn

};


// Druckt UTF-8-Text direkt auf den RA8875 und setzt die vom TP-3000-Font
// verwendeten Sonderzeichen auf die lokalen Einbyte-Codes 0xB0..0xBE um.
// Fuer TextBox-Ausgaben uebernimmt TextBox::print() dieselbe Zuordnung.
void tpTftPrintUtf8(const char* text);

#define TEXTBOXSIZE  1800        // NOTE: Max size of character array (columns * rows) is Fixed at 1800 characters

class TextBox
{
  public:
    //------------------------------------------------------------------------------
    // Init size, shape and colour of Text LCD.
    // variables are columns, rows; upper left corner x, y; Font_name, FG Colour, BG Colour
    void init (int16_t, int16_t, int16_t, int16_t, const ILI9341_t3_font_t &, int16_t, int16_t);
    void init (void) {  font = &DroidSansMono_20; }   // Default settings
    //-----------------------------------------------------------------------------------------
    // Move characters to LCD from virtual LCD, only updates information that has changed
    void transfer(void);
    //-----------------------------------------------------------------------------------------
    // Print to a Virtual LCD - Max LCD_TEXTSIZE character long string representing a Column*Row LCD
    void write(char);                   // Write char
    void print(const char *);           // primitive Print command, no smart formatting
    void setCursor(int16_t, int16_t);   // SetCursor virtual LCD
    void clear(void);                   // Clear virtual LCD
    void invalidate(void);              // Sichtcache ungueltig machen nach TFT-Clear
  
  private:
    int16_t xoffs = 330;                 // Graphics display offset coordinates 
    int16_t yoffs =  80;
    int16_t Col   = 40;                 // Default Columns and Rows
    int16_t Row   = 12;
    int16_t fontXsize = 24;             // Default font size coordinates for an 18 point font
    int16_t fontYsize = 17;
    char    text_lcd[TEXTBOXSIZE+1];    // Character array representing what is visible on LCD
    char    virt_lcd[TEXTBOXSIZE+1];    // Character array representing what is pending for LCD
    int16_t virt_x, virt_y;             // x and y text display coordinates
    const   ILI9341_t3_font_t *font;
    int16_t fontColour = WHITE;
    int16_t blnkColour = BLACK;
};

#endif
