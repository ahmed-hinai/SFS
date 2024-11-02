/*** includes ***/
#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>

/*** dependancies ***/
#define DEPENDANCIES "nbfc-linux, gnuplot" 

/*** defines ***/
#define SFS_VERSION "0.0.1"
#define CTRL_KEY(k) ((k) & 0x1f)
#define CONTROL_MESSAGE  "CTRL+ B: MAX // CTRL+A: AUTO // CTRL+R: RESTART NBFC // CTRL+Q: QUIT\n"

//colors for cli output
#define WHITE "\033[0;39m"
#define RED "\033[31m"
#define GREEN "\033[32m"
#define YELLOW "\033[33m"
#define BLUE "\033[0;36m"
#define ORANGE "\033[0;33m"

enum nbfcStatusKeys {
  IS_READ_ONLY = 0,
  CONFIG_NAME,
  TEMP,
  SKIP1, //nbfc status prints 2 empty new lines. this skips them. this works i guess?
  CPU_FAN,
  CPU_IS_AUTO_ENABLED,
  CPU_IS_CRITICAL_ENABLED,
  CPU_CURRENT_FAN_SPEED,
  CPU_TARGET_FAN_SPEED,
  CPU_FAN_SPEED_STEPS,
  SKIP2,
  GPU_FAN,
  GPU_IS_AUTO_ENABLED,
  GPU_IS_CRITICAL_ENABLED,
  GPU_CURRENT_FAN_SPEED,
  GPU_TARGET_FAN_SPEED,
  GPU_FAN_SPEED_STEPS
};

/*** structs ***/
struct windowConfig {
  int screenrows;
  int screencols;
  struct termios orig_termios;
};


struct windowConfig W;


/*** terminal ***/
void die(const char *s) {
  printf("\x1b[2J");
  printf("\x1b[H");
  perror(s);
  exit(1);


}

void disableRawMode(){
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &W.orig_termios) == -1) die("tcsetattr");

  printf("\33[?25h");
}

void enableRawMode() {

  if (tcgetattr(STDIN_FILENO, &W.orig_termios) == -1) die("tcgetattr");
  atexit(disableRawMode);
 
  struct termios raw = W.orig_termios;
  printf("\33[?25l");
  raw.c_lflag  &= ~(ECHO | ICANON | IEXTEN); //disables echo, canonical mode, and ctrl+v
  raw.c_iflag &= ~(IXON);
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 1; //this is in milliseconds

  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");

}
char readKey() {
  int nread;
  char c;
  while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
    if (nread == -1 && errno != EAGAIN) die("read");
  }
  return c;
}

int getWindowSize(int *rows, int *cols) {
  struct winsize ws;


  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col ==0) {
    return -1;
  } else {
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
  }
}


/*** input ***/
void* processKeypress(){
  char c;
  while((c = readKey()) != EOF) {
    switch (c) {
      case CTRL_KEY('q'):
        printf("\x1b[2J");
        printf("\x1b[H");
        exit(0);
        break;
      case CTRL_KEY('b'):
        if(system("/bin/nbfc set -s 100") != 0) {
        };
        break;
      case CTRL_KEY('a'):
        if(system("/bin/nbfc set -a") != 0) {
        };
        break;
      case CTRL_KEY('r'):
        if (system("/bin/nbfc restart") != 0){//solves some bug that prevents restart
            system("/bin/nbfc stop");
            system("/bin/nbfc start");
        };
        break;
    }
  }
 return NULL;
}

/*** data ***/
char* getNBFCData() {
  FILE* fpipe;
  char* command = "/bin/nbfc status";
  char buffer[64];
  char* output = NULL;
  size_t total_size = 0;
  //open pipe
  if (NULL == (fpipe = popen(command, "r"))){
    perror("popen(). failed.");
    exit(EXIT_FAILURE);
  }
  //read command output
  while (fgets(buffer, sizeof(buffer), fpipe) != NULL) {
    size_t len = strlen(buffer);
    //resize output buffer to fit new data
    char* temp = realloc(output, total_size + len + 1); //need +1 for null terminator
    if (temp == NULL) {
      perror("realloc failed");
      free(output);
      pclose(fpipe);
      return NULL;
    }
    output = temp;
    //copy the buffer to the end of output
    strcpy(output + total_size, buffer);
    total_size += len; 
  }
  if (pclose(fpipe) == -1){
    perror("pclose failed.");
    free(output);
    return NULL;
  }
  return output;
}

char* getLastToken (char* str, const char *delim) {
  char* last_token = NULL;
  char* token = strtok(str,delim);

  while(token != NULL) {
    last_token = token; //update last_token with current token
    token = strtok(NULL, delim); //get next token
  }

  return last_token; //return last token found
}
char* parseNBFCData(int key) {
  const char* raw_data = getNBFCData();
  char* lines[17];
  char line[64];
  int line_count = 0;
  const char* start = raw_data;
  char* data_table[17] = {NULL};
  //split output string into lines
  while(*start && line_count < 17) {
    const char* end = strchr(start, '\n');//strchr to find first occurance of \n i.e end of a line
    if (end) {
      size_t length = end - start;
      if (length >= 64) {
        length = 64 - 1; 
      }
      strncpy(line, start, length);
      line[length] = '\0'; 
      lines[line_count++] = strdup(line);
      start = end + 1; //new start of the next line as after end
    } else {
      break;
    }
  }
  for (int i = 0; i < line_count; i++){
    if (i != 4 && i != 10){//skip the new lines between temp and cpu display name and  between cpu and gpu data. i dont like this
      data_table[i] = getLastToken(lines[i], ":");
    }
  }  
  for (int i = 0; i < line_count; i++) {
    free(lines[i]);
  }
  return data_table[key];
}


void getFanSpeeds(double* buffer) {
  double cpu_fan_speed = atof(parseNBFCData(CPU_CURRENT_FAN_SPEED))*atof(parseNBFCData(CPU_FAN_SPEED_STEPS))/100;
  double gpu_fan_speed = atof(parseNBFCData(GPU_CURRENT_FAN_SPEED))*atof(parseNBFCData(GPU_FAN_SPEED_STEPS))/100;
  buffer[0] = cpu_fan_speed;
  buffer[1] = gpu_fan_speed;
}


/*** output ***/
void printFanSpeed() {
  printf("\n");
  double fan_speeds[2] = {0};
  int space_len = 10;
  getFanSpeeds(fan_speeds);
  
  char command[256];
  snprintf(command, sizeof(command), "echo \"%-*d %-*d\" | figlet -f shadow -w $(tput cols) -c", space_len, (int)fan_speeds[0], space_len, (int)fan_speeds[1]);

  FILE *fp = popen(command, "r");
  if (fp == NULL) {
      perror("popen failed");
      return;
  }

  char buffer[512];
  while (fgets(buffer, sizeof(buffer), fp) != NULL) {
      printf("%s", buffer);
    }
  
  pclose(fp);
}

//U+2591 	░ 	Light shade 
//U+2592 	▒ 	Medium shade
//U+2593 	▓ 	Dark shade 
void drawTempPlot(float Temp, int col, int current_row) {
  int blocks = (int)Temp;
  if (blocks < 10){
    blocks = 10;
  }
  int max_block_height = ((int)(blocks/10))*10 + 10;
  int max_blocks = 10;
  int block_diff = max_block_height - blocks;


  int col_max = W.screencols - 2;
  for (int i = 2; i < col_max; i++){
    printf("\x1b[%d;%dH─",current_row + 1, i);
    printf("\x1b[%d;%dH─",current_row + 12, i);
  }
  for (int i = 0; i <= max_blocks; i++) { // Use < instead of <= to avoid an extra line
    if (i == 0){
      printf("\x1b[%d;2H┌",i+current_row+1);
      printf("\x1b[%d;%dH┐",i+ current_row + 1, col_max);

    }
    if (i == max_blocks){
    printf("\x1b[%d;2H└",i+ current_row + 2);
    printf("\x1b[%d;%dH┘",i+ current_row + 2, col_max);
    }
    else {

      printf("\x1b[%d;2H│",i+ current_row + 2);
      printf("\x1b[%d;%dH│",i+ current_row + 2, col_max);
    }
    if (i < block_diff){
      printf("\x1b[%d;%dH",i + current_row + 1, col); // Move to the correct row and column for each asterisk
      }
     else{

      printf("\x1b[%d;%dH░",i + current_row + 1, col); // Move to the correct row and column for each asterisk
    }
  }
}




void drawSpeedometer(size_t count, size_t max) {

  //write(STDOUT_FILENO, "█\r\n", 3);
  const int bar_width = 50;

  float progress = (float) count / max;
  int bar_length = progress * bar_width;

  printf("\r|");
  for (int i = 0; i < bar_length; ++i) {
      printf("█");
  }
  for (int i = bar_length; i < bar_width; ++i) {
      printf(" ");
  }
  printf("| %.2f%%", progress * 100);

  fflush(stdout);
  }
  
void* refreshScreen(){//needed to modify to work with pthread i.e. void* 
  int cols = W.screencols;
  int rows = W.screenrows; 
  int control_len = strlen(CONTROL_MESSAGE);
  int padding = 2;
  int col_count_cpu = 3;
  int col_count_gpu = 3;
  while (1) {
    //print control message
    printf("\x1b[1J");
    printf("\x1b[1;%dH", (cols - control_len)/2); 
    printf(CONTROL_MESSAGE);
    //print fan speed
    printf("\x1b[3;1H");//move to third row
    //printf("\x1b[1;32m"); <-- makes everything green!
    printFanSpeed();
    printf("\x1b[%d;1H", rows);
    //print temp/util graph
    //printf("\x1b[9;%dH", col_count);
    drawTempPlot(atof(parseNBFCData(TEMP)), col_count_cpu, 10); 
    if (col_count_cpu < cols - padding) {
      col_count_cpu++;
    } else {
      col_count_cpu = 3;
      printf("\x1b[11;3H");
      for (int i = 0; i < 10; i++){
        printf("\x1b[B\x1b[K");
      } 
    }

    drawTempPlot(atof(parseNBFCData(TEMP)), col_count_gpu, rows - 22); 
    if (col_count_gpu < cols - padding) {
      col_count_gpu++;
    } else {
      col_count_gpu = 3;
      printf("\x1b[%d;3H", rows - 21);
      for (int i = 0; i < 10; i++){
        printf("\x1b[B\x1b[K");
      } 
    }
    printf("\x1b[H");
    fflush(stdout);
    usleep(1000000);
}
  return NULL;
}



/*** init ***/
void initWindow() {
  if (getWindowSize(&W.screenrows, &W.screencols) == -1) die("getWindowSize");
}
int main() {
  enableRawMode();
  initWindow();
  pthread_t key_thread, print_thread;
  //this creates two threads for reading keys and printing to terminal
  pthread_create(&key_thread, NULL, processKeypress, NULL);
  pthread_create(&print_thread, NULL, refreshScreen, NULL);
  //wait for the threads to finish
  pthread_join(key_thread, NULL);
  pthread_join(print_thread, NULL);
 return 0;
}
