/*
  Blank Simple Project.c
  http://learn.parallax.com/propeller-c-tutorials 
*/
#include "simpletools.h"                      // Include simple tools
#include "propeller.h"     // Include low-level Propeller functions
#include "adcDCpropab.h"

#define BUTTON_PIN    1  // Define the button input pin
#define TIMEOUT       2000  // 2 seconds in milliseconds
#define MAZE_ROWS     19
#define MAZE_COLS     15
#define MPU6050_ADDR  0x68  // I2C address of MPU6050
#define PWR_MGMT_1    0x6B  // Power management register
#define ACCEL_XOUT_H  0x3B  // Accelerometer data register
#define WHO_AM_I      0x75  // MPU6050 WHO_AM_I register
#define I2C_SCL       15
#define I2C_SDA       14
#define TILT_SENS     3000
#define GAME_TIMEOUT  60

enum game_state_e {
  TITLE_SCREEN,
  LEVEL_SCREEN,
  MAZE_GENERATION,
  GAME_PLAY,
  GAME_WIN,
  GAME_OVER,
  GAME_END
};

const int win_ipl_tune_freq[] = {2349, 2489, 2794, 2489, 2349, 2093, 2349, 2489, 2349};
const int win_ipl_tune_time[] = {270, 270, 270, 270, 270, 270, 270, 270, 600};
const int win_ipl_arr_size = sizeof(win_ipl_tune_freq) / sizeof(win_ipl_tune_freq[0]);

const int game_over_tune_freq[] = {3136, 2637, 2093, 1568, 1047};
const int game_over_tune_time[] = {300, 300, 400, 400, 600};
const int game_over_arr_size = sizeof(game_over_tune_freq) / sizeof(game_over_tune_freq[0]);

serial *lcd;
i2c *i2c_bus;
int ball_x = 1;
int ball_y = 1;
unsigned char current_level = 1;
unsigned char game_state = 0;
unsigned char maze_current[MAZE_ROWS][MAZE_COLS];

int prev_game_timer = 0;
volatile int game_timer = 0;
volatile unsigned char timer_state = 0;
volatile char temp_cmd = 0;
volatile unsigned char serial_inp = 0;
volatile unsigned char sound_state = 0;

void write_cmd_to_lcd(serial *lcd_ptr, char *str);
void update_screen(char cmd);
void update_time(int seconds);
void update_battery(int percent);
void update_cell(int x, int y, char color);
void update_ball(int x, int y, char color);
void clear_ball(int x, int y);
int is_valid_move(int x, int y);
void move_ball(char motion_cmd);
void shuffle(int arr[][2], int n);
void carve_passages(int x, int y);
void generate_maze();
void display_maze();
void mpu6050_init();
void read_accel(int *ax, int *ay, int *az);
void determine_tilt(int ax, int ay, int az);
void play_sound(const int *arr_freq, const int *arr_time, const int arr_size);
void game_loop();
void decrement_timer_loop();
void imu_loop();
void play_sound_loop();
void button_handler_loop();

int main() {
  cog_run(game_loop, 128);
  cog_run(decrement_timer_loop, 128);
  cog_run(imu_loop, 128);
  cog_run(play_sound_loop, 128);
  cog_run(button_handler_loop, 128);
}  

void game_loop()// Main function
{
  // Add startup code here.
  lcd = serial_open(12, 13, 0, 9600); //RX - TX - INV - BAUDRATE
  
  while (1) {    
    switch (game_state) {
      case TITLE_SCREEN:
        update_screen('S');
        pause(2000);
        current_level = 1;
        game_state = LEVEL_SCREEN;
        break;
      case LEVEL_SCREEN:
        update_screen(current_level + 48);
        pause(2000);
        update_screen('A');
        pause(3000);
        game_state = MAZE_GENERATION;
        break;
      case MAZE_GENERATION:
        generate_maze();
        display_maze();
        ball_x = 1;
        ball_y = 1;
        update_ball(ball_x, ball_y, 'P');
        game_timer = GAME_TIMEOUT;
        timer_state = 0;
        game_state = GAME_PLAY;
        break;
      case GAME_PLAY:
        if (serial_inp) {
          move_ball(temp_cmd);
          timer_state = 1;
          serial_inp = 0;
        }
        if (prev_game_timer != game_timer) {
          update_time(game_timer);
          prev_game_timer = game_timer;
          if (game_timer == 0) {
            update_screen('O');
            sound_state = 2;
            game_state = GAME_OVER;
          }            
        }
        
        if (maze_current[ball_y][ball_x] == 3) {
          game_state = GAME_WIN;
        }
        break;
      case GAME_WIN:
        update_screen('L');
        sound_state = 1;
        pause(3000);
        current_level++;
        game_state = LEVEL_SCREEN;
        break;
      case GAME_OVER:
        pause(500);
        break;
      case GAME_END:
        pause(500);
        break;
    }         
  }
}
  

void decrement_timer_loop() {
  while(1) {
    while(timer_state) {
      if (game_timer > 0) { 
        game_timer--;  // Decrement the shared variable
        pause(1000);  // Decrement every 1000ms
      }     
    }
  }  
}

void imu_loop() {
  i2c_bus = i2c_newbus(I2C_SCL, I2C_SDA, 0);
  mpu6050_init();
  while(1)
  {
    int ax, ay, az;
    read_accel(&ax, &ay, &az);
    determine_tilt(ax, ay, az);
    pause(200);
  }
}

void play_sound_loop() {
  while(1) {
    if (sound_state == 1) {
      play_sound(win_ipl_tune_freq, win_ipl_tune_time, win_ipl_arr_size);
      sound_state = 0;
    }
    else if (sound_state == 2) {
      play_sound(game_over_tune_freq, game_over_tune_time, game_over_arr_size);
      sound_state = 0;
    }      
  }
}

void button_handler_loop()
{
  int buttonState;
  unsigned int startTime, pressDuration;

  while (1) {
    // Wait for button press
    while (input(BUTTON_PIN) == 0); // Button not pressed
    startTime = CNT; // Capture the time when button is pressed

    // Wait for button release
    while (input(BUTTON_PIN) == 1); // Button is pressed

    pressDuration = (CNT - startTime) / (CLKFREQ / 1000); // Convert to milliseconds

    if (pressDuration < TIMEOUT)
    {
      // Call function or action for X
      game_state = LEVEL_SCREEN;
    }
    else
    {
      // Call function or action for Y
      game_state = TITLE_SCREEN;
    }

    pause(500); // Small delay to avoid debounce issues
  }
}

void write_cmd_to_lcd(serial *lcd_ptr, char *str) {
  for (int i = 0; i < strlen(str); i++) {
    writeChar(lcd_ptr, str[i]);
  }    
}

void update_screen(char cmd) {
  char cmd_str[10];
  
  sprintf(cmd_str, "S %c\n", cmd);
  write_cmd_to_lcd(lcd, cmd_str);
}

void update_time(int seconds) {
  char cmd_str[10];
  
  sprintf(cmd_str, "T %d\n", seconds);
  write_cmd_to_lcd(lcd, cmd_str);
}

void update_cell(int x, int y, char color) {
  char cmd_str[10];
  
  sprintf(cmd_str, "M %d %d %c\n", x, y, color);
  write_cmd_to_lcd(lcd, cmd_str);
}

void update_ball(int x, int y, char color) {
  char cmd_str[10];
  
  sprintf(cmd_str, "B %d %d %c\n", x, y, color);
  write_cmd_to_lcd(lcd, cmd_str);
}

void clear_ball(int x, int y) {
    char color;
    if (maze_current[y][x] == 1) {
        color = 'K';
    } else if (maze_current[y][x] == 2) {
        color = 'R';
    } else if (maze_current[y][x] == 3) {
        color = 'G';
    } else {
        color = 'W';
    }
    update_cell(x, y, color);
}

int is_valid_move(int x, int y) {
  return (x >= 0 && x < MAZE_COLS && y >= 0 && y < MAZE_ROWS && maze_current[y][x] != 1);
}

void move_ball(char motion_cmd) {
  int new_x = ball_x;
  int new_y = ball_y;
  
  if (motion_cmd == 'W') new_y--;
  else if (motion_cmd == 'S') new_y++;
  else if (motion_cmd == 'A') new_x--;
  else if (motion_cmd == 'D') new_x++;
   
  if (is_valid_move(new_x, new_y)) {
    clear_ball(ball_x, ball_y);
    ball_x = new_x;
    ball_y = new_y;
    update_ball(ball_x, ball_y, 'P');
  }
}

// Function to shuffle movement directions
void shuffle(int arr[][2], int n) {
    for (int i = n - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        int tempX = arr[i][0], tempY = arr[i][1];
        arr[i][0] = arr[j][0]; arr[i][1] = arr[j][1];
        arr[j][0] = tempX; arr[j][1] = tempY;
    }
}

// Recursive function to carve paths in the maze
void carve_passages(int x, int y) {
    int directions[4][2] = { {0, 2}, {2, 0}, {0, -2}, {-2, 0} };
    shuffle(directions, 4);  // Shuffle the directions randomly

    for (int i = 0; i < 4; i++) {
        int dx = directions[i][0];
        int dy = directions[i][1];
        int nx = x + dx, ny = y + dy;

        if (nx > 0 && nx < MAZE_ROWS - 1 && ny > 0 && ny < MAZE_COLS - 1 && maze_current[nx][ny] == 1) {
            maze_current[x + dx / 2][y + dy / 2] = 0;  // Remove wall between
            maze_current[nx][ny] = 0;  // Open path
            carve_passages(nx, ny);
        }
    }
}

// Function to generate the maze
void generate_maze() {
  // Initialize maze with walls
  for (int i = 0; i < MAZE_ROWS; i++) {
    for (int j = 0; j < MAZE_COLS; j++) {
      maze_current[i][j] = 1;
    }
  }

  // Start maze generation from (1,1)
  maze_current[1][1] = 0;
  carve_passages(1, 1);

  // Set start and end points
  maze_current[1][1] = 2;  // Start
  maze_current[MAZE_ROWS - 2][MAZE_COLS - 2] = 3;  // End
}

void display_maze() {
  for (int i = 0; i < MAZE_COLS; i++) {  // Iterate through rows
    for (int j = 0; j < MAZE_ROWS; j++) {  // Iterate through columns
      if (maze_current[j][i] == 1) {
        update_cell(i, j, 'K');  // Walls (Black)
      } else if (maze_current[j][i] == 0) {
        update_cell(i, j, 'W');  // Open path (White)
      } else if (maze_current[j][i] == 2) {
        update_cell(i, j, 'R');  // Start position (Red)
      } else if (maze_current[j][i] == 3) {
        update_cell(i, j, 'G');  // End position (Green)
      }
    }
  }    
}  

int16_t convert_to_signed(uint16_t value)
{
    if (value > 32767)  // Check for negative values
        return value - 65536;  // Convert to negative
    else
        return value;  // Return positive values as is
}

void mpu6050_init()
{
    unsigned char whoami;
    i2c_in(i2c_bus, MPU6050_ADDR, WHO_AM_I, 1, &whoami, 1);
    // Wake up MPU6050
    unsigned char wakeup_data = 0x00;
    i2c_out(i2c_bus, MPU6050_ADDR, PWR_MGMT_1, 1, &wakeup_data, 1);
}

void read_accel(int *ax, int *ay, int *az)
{
    unsigned char data[6];

    // Read accelerometer data starting from ACCEL_XOUT_H
    i2c_in(i2c_bus, MPU6050_ADDR, ACCEL_XOUT_H, 1, data, 6);

    // Combine high and low bytes and convert to signed int
    *ax = convert_to_signed((data[0] << 8) | data[1]);
    *ay = convert_to_signed((data[2] << 8) | data[3]);
    *az = convert_to_signed((data[4] << 8) | data[5]);
}

void determine_tilt(int ax, int ay, int az)
{
  if (ax > TILT_SENS) {
    // Right Tilt Condition
    temp_cmd = 'W';//W - D
    serial_inp = 1;
  }        
  else if (ax < -TILT_SENS) {
    // Left Tilt Condition
    temp_cmd = 'S';//S - A
    serial_inp = 1;
  }
  else if (ay > TILT_SENS){
    // Forward Tilt Condition
    temp_cmd = 'D';//D - W
    serial_inp = 1;
  }
  else if (ay < -TILT_SENS){
    // Backward Tilt Condition
    temp_cmd = 'A';//A - S
    serial_inp = 1;
  }
  else{
    // Level Condition
  }
}

void play_sound(const int *arr_freq, const int *arr_time, const int arr_size) {
  for(int i = 0; i < arr_size; i++)
  {
    freqout(0, arr_time[i], arr_freq[i]);
  }
}
