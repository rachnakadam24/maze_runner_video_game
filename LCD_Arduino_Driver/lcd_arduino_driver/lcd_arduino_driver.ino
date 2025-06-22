#include <LCDWIKI_GUI.h>  // Core graphics library
#include <LCDWIKI_SPI.h>  // Hardware-specific library
#include <SoftwareSerial.h>

#define MODEL ST7796S
#define CS   10    
#define CD   9
#define RST  8
#define LED  5    

#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 480
#define CELL_SIZE  20
#define MAZE_ROWS (((SCREEN_HEIGHT-80) / CELL_SIZE) - 1)
#define MAZE_COLS ((SCREEN_WIDTH / CELL_SIZE) - 1)

LCDWIKI_SPI mylcd(MODEL, CS, CD, RST, LED);

enum Colors {
  BLACK   = 0x0000,
  WHITE   = 0xFFFF,
  RED     = 0xF800,
  GREEN   = 0x07E0,
  BLUE    = 0x001F,
  CYAN    = 0x07FF,
  MAGENTA = 0xF81F,
  YELLOW  = 0xFFE0,
  ORANGE  = 0xFD20,
  PURPLE  = 0x780F,
  GRAY    = 0x8410
};

#define MAX_BUFFER_SIZE 100  // Maximum size of input buffer

char inputBuffer[MAX_BUFFER_SIZE];  // Array to store input
int bufferIndex = 0;
const byte rxPin = 2;
const byte txPin = 3;

SoftwareSerial mySerial (rxPin, txPin);

void setup() {
  mySerial.begin(9600);
  mylcd.Init_LCD();
}

void loop() {
  uint8_t cell_type = 0;

  if (readSerialInput(inputBuffer, MAX_BUFFER_SIZE)) {
    char* token = strtok(inputBuffer, " ");

    if (token != NULL) {
      if (strcmp(token, "S") == 0) {
        // Expected format: S L
        token = strtok(NULL, " "); // Get the second token
        if (token != NULL && strcmp(token, "S") == 0) {
          startUpScreen();
        }
        else if (token != NULL && strcmp(token, "L") == 0) {
          levelClearedScreen();
        }
        else if (token != NULL && strcmp(token, "W") == 0) {
          gameCompleteScreen();
        }
        else if (token != NULL && strcmp(token, "O") == 0) {
          gameOverScreen();
        }
        else if (token != NULL && strcmp(token, "A") == 0) {
          gameArenaScreen();
        }
        if (token != NULL) {
          int level = atoi(token);
          gameLevelScreen(level);
        }
      }
      else if (strcmp(token, "T") == 0) {
        token = strtok(NULL, " ");
        if (token != NULL) {
          int inp_time = atoi(token);
          printTimer(inp_time);
        }
      }
      else if ((strcmp(token, "M") == 0) || (strcmp(token, "B") == 0)) {
        if (strcmp(token, "M") == 0) {
          cell_type = 1;
        }
        else if (strcmp(token, "B") == 0) {
          cell_type = 2;
        }
        // Expected format: M X Y C
        token = strtok(NULL, " "); // Get X position
        if (token != NULL) {
          int inp_x = atoi(token); // Convert string to integer

          token = strtok(NULL, " "); // Get Y position
          if (token != NULL) {
            int inp_y = atoi(token); // Convert string to integer

            token = strtok(NULL, " "); // Get color identifier
            if (token != NULL) {
              uint16_t color = getColorFromChar(token[0]); // Convert color character to color value
              if (cell_type == 1) {
                drawCell(inp_x, inp_y, color);
              }
              else if (cell_type == 2) {
                drawBall(inp_x, inp_y, color);
              }
            }
          }
        }
      }
    }
  }
}

uint16_t getColorFromChar(char colorChar) {
  switch (colorChar) {
    case 'K': return BLACK;
    case 'W': return WHITE;
    case 'R': return RED;
    case 'G': return GREEN;
    case 'B': return BLUE;
    case 'C': return CYAN;
    case 'M': return MAGENTA;
    case 'Y': return YELLOW;
    case 'O': return ORANGE;
    case 'P': return PURPLE;
    case 'A': return GRAY;
    default: return BLACK;
  }
}

bool readSerialInput(char* buffer, int maxLength) {
    while (mySerial.available()) {
        char incomingByte = mySerial.read();

        // Check for newline (end of input)
        if (incomingByte == '\n') {
            buffer[bufferIndex] = '\0';  // Null-terminate the string
            bufferIndex = 0;  // Reset index for next input
            return true;  // Input complete
        }

        // Store byte if buffer isn't full
        if (bufferIndex < maxLength - 1) {
            buffer[bufferIndex++] = incomingByte;
        }
    }
    return false;  // Input not complete yet
}

void drawCenteredText(const char* text, uint16_t textColor, uint16_t textBgColor, uint8_t textSize, uint16_t bgColor) {
  mylcd.Fill_Screen(bgColor);  // Clear screen with background color
  mylcd.Set_Text_Mode(0);      // Set text mode
  mylcd.Set_Text_colour(textColor);
  mylcd.Set_Text_Back_colour(textBgColor);
  mylcd.Set_Text_Size(textSize);

  // Character dimensions based on text size
  int char_width = 6 * textSize;  
  int char_height = 8 * textSize; 

  // Calculate position to center text
  int text_x = (SCREEN_WIDTH - (strlen(text) * char_width)) / 2;
  int text_y = (SCREEN_HEIGHT - char_height) / 2;

  // Print centered text
  mylcd.Print_String(text, text_x, text_y);
}

void drawCell(int x, int y, uint16_t color) {
  int pixel_x = (x * CELL_SIZE) + 10;
  int pixel_y = (y * CELL_SIZE) + 10;
  mylcd.Fill_Rect(pixel_x, pixel_y, CELL_SIZE, CELL_SIZE, color);
}

void drawBall(int x, int y, uint16_t color) {
    mylcd.Set_Draw_color(color);
    mylcd.Fill_Circle((x * CELL_SIZE + CELL_SIZE / 2) + 10, (y * CELL_SIZE + CELL_SIZE / 2) + 10, CELL_SIZE / 3);
}

void drawConfetti(int numParticles) {
  for (int i = 0; i < numParticles; i++) {
    int x = random(0, mylcd.Get_Display_Width());
    int y = random(0, mylcd.Get_Display_Height());
    uint16_t color = mylcd.Color_To_565(random(0, 255), random(0, 255), random(0, 255));
    mylcd.Draw_Pixe(x, y, color);
    delay(10);
  }
}

void startUpScreen() {
  drawCenteredText("MAZE RUNNER", BLUE, WHITE, 4, WHITE);
}

void gameLevelScreen(uint8_t levelNo) {
  char printStr[10];
  sprintf(printStr, "Level %d", levelNo);
  drawCenteredText(printStr, PURPLE, WHITE, 3  , WHITE);
}

void gameOverScreen() {
  drawCenteredText("Game Over", YELLOW, RED, 3  , RED);
}

void levelClearedScreen() {
  drawCenteredText("Level Cleared", BLACK, GREEN, 3, GREEN);
}

void gameCompleteScreen() {
  drawCenteredText("YOU WIN!!!", WHITE, BLACK, 3  , BLACK);
  for (int i = 0; i < 30; i++) { // Generate confetti animation
    drawConfetti(20);
    delay(100);
  }
}

void gameArenaScreen() {
  mylcd.Fill_Screen(BLACK);
  mylcd.Fill_Rect(5, 5, 310, 390, WHITE);
}

void printTimer(int seconds) {
  mylcd.Set_Draw_color(RED);
  mylcd.Set_Text_Mode(0);
  mylcd.Set_Text_colour(RED);
  mylcd.Set_Text_Back_colour(BLACK); 
  mylcd.Set_Text_Size(3);

  int text_x = (SCREEN_WIDTH - (5 * 18)) / 2;
  int text_y = 400 + (80 / 2) - 12;
  
  char timeStr[10];
  int time_min = seconds / 60;
  int time_sec = seconds % 60;
  sprintf(timeStr, "%02d:%02d", time_min, time_sec);
  
  mylcd.Print_String(timeStr, text_x, text_y);
}

void drawCheckeredPattern(uint16_t color1, uint16_t color2) {
  for (int y = 0; y < 19; y++) {  // 19 rows
    for (int x = 0; x < 15; x++) { // 15 columns
      uint16_t color = ((x + y) % 2 == 0) ? color1 : color2; // Alternate colors
      drawCell(x, y, color);
    }
  }
}
