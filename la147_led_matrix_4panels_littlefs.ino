#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <Fonts/FreeSerifBold12pt7b.h>

#include "FS.h"
#include <LittleFS.h>
#include <AnimatedGIF.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

#define FILESYSTEM LittleFS
#define FORMAT_LITTLEFS_IF_FAILED true

AnimatedGIF gif;
File f;
int x_offset, y_offset;

const char *texte = "Course enfants";

// Configurations des pins pour le panneau
#define R1_PIN 42
#define G1_PIN 41
#define B1_PIN 40
#define R2_PIN 38
#define G2_PIN 39
#define B2_PIN 37
#define A_PIN 45
#define B_PIN 36
#define C_PIN 48
#define D_PIN 35
#define E_PIN 21
#define LAT_PIN 47
#define OE_PIN 14
#define CLK_PIN 2

HUB75_I2S_CFG::i2s_pins _pins = { R1_PIN, G1_PIN, B1_PIN, R2_PIN, G2_PIN, B2_PIN, A_PIN, B_PIN, C_PIN, D_PIN, E_PIN, LAT_PIN, OE_PIN, CLK_PIN };
/******************************************************************************
    -------------------------------------------------------------------------
    Steps to create a virtual display made up of a chain of panels in a grid
    -------------------------------------------------------------------------

    Read the documentation!
    https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-DMA/tree/master/examples/ChainedPanels

    tl/dr:
    
    - Set values for NUM_ROWS, NUM_COLS, PANEL_RES_X, PANEL_RES_Y, PANEL_CHAIN_TYPE. 

    - Other than where the matrix is defined and matrix.begin in the setup, you 
      should now be using the virtual display for everything (drawing pixels, writing text etc).
       You can do a find and replace of all calls if it's an existing sketch
      (just make sure you don't replace the definition and the matrix.begin)

    - If the sketch makes use of MATRIX_HEIGHT or MATRIX_WIDTH, these will need to be 
      replaced with the width and height of your virtual screen. 
      Either make new defines and use that, or you can use virtualDisp.width() or .height()

*****************************************************************************/
// 1) Include key virtual display library
#include <ESP32-VirtualMatrixPanel-I2S-DMA.h>

// 2) Set configuration
#define PANEL_RES_X 64  // Number of pixels wide of each INDIVIDUAL panel module.
#define PANEL_RES_Y 32  // Number of pixels tall of each INDIVIDUAL panel module.

#define NUM_ROWS 2                      // Number of rows of chained INDIVIDUAL PANELS
#define NUM_COLS 2                      // Number of INDIVIDUAL PANELS per ROW
#define PANEL_CHAIN NUM_ROWS *NUM_COLS  // total number of panels chained one to another

/* Configure the serpetine chaining approach. Options are:
        CHAIN_TOP_LEFT_DOWN
        CHAIN_TOP_RIGHT_DOWN
        CHAIN_BOTTOM_LEFT_UP
        CHAIN_BOTTOM_RIGHT_UP

        The location (i.e. 'TOP_LEFT', 'BOTTOM_RIGHT') etc. refers to the starting point where 
        the ESP32 is located, and how the chain of panels will 'roll out' from there.

        In this example we're using 'CHAIN_BOTTOM_LEFT_UP' which would look like this in the real world:

        Chain of 4 x 64x32 panels with the ESP at the BOTTOM_LEFT:

        +---------+---------+
        |    4    |    3    |
        |         |         |
        +---------+---------+
        |    1    |    2    |
        |  (ESP)  |         |
        +---------+---------+      

    */
#define VIRTUAL_MATRIX_CHAIN_TYPE CHAIN_TOP_RIGHT_DOWN

// 3) Create the runtime objects to use

// placeholder for the matrix object
MatrixPanel_I2S_DMA *dma_display = nullptr;

// placeholder for the virtual display object
VirtualMatrixPanel *virtualDisp = nullptr;


void GIFDraw(GIFDRAW *pDraw) {
  uint8_t *s;
  uint16_t *d, *usPalette, usTemp[320];
  int x, y, iWidth;

  iWidth = pDraw->iWidth;
  if (iWidth > virtualDisp->width())
    iWidth = virtualDisp->width();

  usPalette = pDraw->pPalette;
  y = pDraw->iY + pDraw->y;  // current line

  s = pDraw->pPixels;
  if (pDraw->ucDisposalMethod == 2)  // restore to background color
  {
    for (x = 0; x < iWidth; x++) {
      if (s[x] == pDraw->ucTransparent)
        s[x] = pDraw->ucBackground;
    }
    pDraw->ucHasTransparency = 0;
  }
  // Apply the new pixels to the main image
  if (pDraw->ucHasTransparency)  // if transparency used
  {
    uint8_t *pEnd, c, ucTransparent = pDraw->ucTransparent;
    int x, iCount;
    pEnd = s + pDraw->iWidth;
    x = 0;
    iCount = 0;  // count non-transparent pixels
    while (x < pDraw->iWidth) {
      c = ucTransparent - 1;
      d = usTemp;
      while (c != ucTransparent && s < pEnd) {
        c = *s++;
        if (c == ucTransparent)  // done, stop
        {
          s--;  // back up to treat it like transparent
        } else  // opaque
        {
          *d++ = usPalette[c];
          iCount++;
        }
      }            // while looking for opaque pixels
      if (iCount)  // any opaque pixels?
      {
        for (int xOffset = 0; xOffset < iCount; xOffset++) {
          virtualDisp->drawPixel(x + xOffset, y, usTemp[xOffset]);  // 565 Color Format
        }
        x += iCount;
        iCount = 0;
      }
      // no, look for a run of transparent pixels
      c = ucTransparent;
      while (c == ucTransparent && s < pEnd) {
        c = *s++;
        if (c == ucTransparent)
          iCount++;
        else
          s--;
      }
      if (iCount) {
        x += iCount;  // skip these
        iCount = 0;
      }
    }
  } else  // does not have transparency
  {
    s = pDraw->pPixels;
    // Translate the 8-bit pixels through the RGB565 palette (already byte reversed)
    for (x = 0; x < pDraw->iWidth; x++) {
      virtualDisp->drawPixel(x, y, usPalette[*s++]);  // color 565
    }
  }
} /* GIFDraw() */

void *GIFOpenFile(const char *fname, int32_t *pSize) {
  Serial.print("Playing gif: ");
  Serial.println(fname);
  f = FILESYSTEM.open(fname);
  if (f) {
    *pSize = f.size();
    return (void *)&f;
  }
  return NULL;
} /* GIFOpenFile() */

void GIFCloseFile(void *pHandle) {
  File *f = static_cast<File *>(pHandle);
  if (f != NULL)
    f->close();
} /* GIFCloseFile() */

int32_t GIFReadFile(GIFFILE *pFile, uint8_t *pBuf, int32_t iLen) {
  int32_t iBytesRead;
  iBytesRead = iLen;
  File *f = static_cast<File *>(pFile->fHandle);
  // Note: If you read a file all the way to the last byte, seek() stops working
  if ((pFile->iSize - pFile->iPos) < iLen)
    iBytesRead = pFile->iSize - pFile->iPos - 1;  // <-- ugly work-around
  if (iBytesRead <= 0)
    return 0;
  iBytesRead = (int32_t)f->read(pBuf, iBytesRead);
  pFile->iPos = f->position();
  return iBytesRead;
} /* GIFReadFile() */

int32_t GIFSeekFile(GIFFILE *pFile, int32_t iPosition) {
  int i = micros();
  File *f = static_cast<File *>(pFile->fHandle);
  f->seek(iPosition);
  pFile->iPos = (int32_t)f->position();
  i = micros() - i;
  //  Serial.printf("Seek time = %d us\n", i);
  return pFile->iPos;
} /* GIFSeekFile() */

unsigned long start_tick = 0;

void ShowGIF(char *name) {
  start_tick = millis();

  if (gif.open(name, GIFOpenFile, GIFCloseFile, GIFReadFile, GIFSeekFile, GIFDraw)) {
    x_offset = (virtualDisp->width() - gif.getCanvasWidth()) / 2;
    if (x_offset < 0) x_offset = 0;
    y_offset = (virtualDisp->height() - gif.getCanvasHeight()) / 2;
    if (y_offset < 0) y_offset = 0;
    Serial.printf("Successfully opened GIF; Canvas size = %d x %d\n", gif.getCanvasWidth(), gif.getCanvasHeight());
    Serial.flush();
    while (gif.playFrame(true, NULL)) {
/*      if ((millis() - start_tick) > 8000) {  // we'll get bored after about 8 seconds of the same looping gif
        break;
      }
*/
    }

    gif.close();
  }

} /* ShowGIF() */

void setup() {

  delay(2000);
      // Initialisation de LittleFS
    if (!LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED)) {
        Serial.println("Échec de l'initialisation de LittleFS");
        return; // Sortir de setup si l'initialisation échoue
    }
  Serial.begin(115200);
  Serial.println("");
  Serial.println("");
  Serial.println("");
  Serial.println("*****************************************************");
  Serial.println(" HELLO !");
  Serial.println("*****************************************************");


  /******************************************************************************
   * Create physical DMA panel class AND virtual (chained) display class.   
   ******************************************************************************/

  /*
    The configuration for MatrixPanel_I2S_DMA object is held in HUB75_I2S_CFG structure,
    All options has it's predefined default values. So we can create a new structure and redefine only the options we need

	Please refer to the '2_PatternPlasma.ino' example for detailed example of how to use the MatrixPanel_I2S_DMA configuration
  */

    HUB75_I2S_CFG mxconfig(
      PANEL_RES_X,  // module width
      PANEL_RES_Y,  // module height
      PANEL_CHAIN,  // chain length
      _pins);

  mxconfig.clkphase = false;
  //mxconfig.driver = HUB75_I2S_CFG::FM6126A;     // in case that we use panels based on FM6126A chip, we can set it here before creating MatrixPanel_I2S_DMA object

  // Sanity checks
  if (NUM_ROWS <= 1) {
    Serial.println(F("There is no reason to use the VirtualDisplay class for a single horizontal chain and row!"));
  }

  // OK, now we can create our matrix object
  dma_display = new MatrixPanel_I2S_DMA(mxconfig);

  // let's adjust default brightness to about 75%
  dma_display->setBrightness8(192);  // range is 0-255, 0 - 0%, 255 - 100%

  // Allocate memory and start DMA display
  if (not dma_display->begin())
    Serial.println("****** !KABOOM! I2S memory allocation failed ***********");

  // create VirtualDisplay object based on our newly created dma_display object
  virtualDisp = new VirtualMatrixPanel((*dma_display), NUM_ROWS, NUM_COLS, PANEL_RES_X, PANEL_RES_Y, VIRTUAL_MATRIX_CHAIN_TYPE);

  // So far so good, so continue
  virtualDisp->fillScreen(virtualDisp->color444(0, 0, 0));
  virtualDisp->drawDisplayTest();  // draw text numbering on each screen to check connectivity

  // delay(1000);

  Serial.println("Chain of 4x 64x32 panels for this example:");
  Serial.println("+---------+---------+");
  Serial.println("|    4    |    3    |");
  Serial.println("|         |         |");
  Serial.println("+---------+---------+");
  Serial.println("|    1    |    2    |");
  Serial.println("| (ESP32) |         |");
  Serial.println("+---------+---------+");

  // draw blue text
  virtualDisp->setFont(&FreeSansBold12pt7b);
  virtualDisp->setTextColor(virtualDisp->color565(0, 0, 255));
  virtualDisp->setTextSize(3);
  virtualDisp->setCursor(0, virtualDisp->height() - ((virtualDisp->height() - 45) / 2));
  virtualDisp->print("ABCD");

  // Red text inside red rect (2 pix in from edge)
  virtualDisp->drawRect(1, 1, virtualDisp->width() - 2, virtualDisp->height() - 2, virtualDisp->color565(255, 0, 0));

  // White line from top left to bottom right
  virtualDisp->drawLine(0, 0, virtualDisp->width() - 1, virtualDisp->height() - 1, virtualDisp->color565(255, 255, 255));

  virtualDisp->drawDisplayTest();  // re draw text numbering on each screen to check connectivity
  delay(2000);


  virtualDisp->fillScreen(virtualDisp->color444(0, 0, 0));
  virtualDisp->drawRect(1, 1, virtualDisp->width() - 2, virtualDisp->height() - 2, virtualDisp->color565(255, 0, 0));
  virtualDisp->drawRect(0, 0, virtualDisp->width(), virtualDisp->height(), virtualDisp->color565(0, 255, 0));
  virtualDisp->setFont(&FreeSerifBold12pt7b);
  virtualDisp->setTextColor(virtualDisp->color565(0, 0, 255));



  virtualDisp->fillScreen(virtualDisp->color444(0, 0, 0));
  virtualDisp->drawPixel(0, 0, virtualDisp->color444(0, 255, 0));
  virtualDisp->drawPixel((virtualDisp->width() - 1), 0, virtualDisp->color444(0, 255, 0));
  virtualDisp->drawPixel((virtualDisp->width() - 1), (virtualDisp->height() - 1), virtualDisp->color444(0, 255, 0));
  virtualDisp->drawPixel(0, (virtualDisp->height() - 1), virtualDisp->color444(0, 255, 0));

delay(2000);

//virtualDisp->fillScreen(virtualDisp->color444(0, 0, 0));
  

  // Start going through GIFS
  gif.begin(LITTLE_ENDIAN_PIXELS);
}

String gifDir = "/gifs";  // play all GIFs in this directory on the SD card
char filePath[256] = { 0 };
File root, gifFile;

void loop() {
    while (1)  // run forever
    {

      root = FILESYSTEM.open(gifDir);
      Serial.print("Tentative ouverture  ");
      Serial.println(gifDir);
      if (root) {
        Serial.println("C est bon");
        gifFile = root.openNextFile();
        while (gifFile) {
          if (!gifFile.isDirectory())  // play it
          {

            // C-strings... urghh...
            memset(filePath, 0x0, sizeof(filePath));
            strcpy(filePath, gifFile.path());

            // Show it.
             Serial.print("Fichier : ");
             Serial.print(filePath);
            ShowGIF(filePath);
          }
          gifFile.close();
          gifFile = root.openNextFile();
        }
        root.close();
      }  // root

      delay(1000);  // pause before restarting

    }  // while

  }  // end loop
