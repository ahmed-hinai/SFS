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

void getGpuTemp(char* temp){
  char buffer[4];
  FILE* fp = popen("nvidia-smi -q | awk '/^ *GPU Current Temp/ {print $(NF-1)}'","r");
  if (fp == NULL){
    perror("popen(). failed");
    return;
  }
  while(fgets(buffer, sizeof(buffer), fp) != NULL){
    strncpy(temp, buffer, 4);
  }
  pclose(fp);
}

void getGpuUtil(char* util){
  char buffer[4];
  FILE* fp = popen("nvidia-smi -q | awk '/Utilization/{getline; print $(NF-1)}'", "r");
  if (fp == NULL){
    perror("popen(). failed");
    return;
  }
  while(fgets(buffer, sizeof(buffer), fp) != NULL){
    strncpy(util, buffer, 4);

  }
  pclose(fp);

}


/*** output ***/
void printHeader(){
  int cols = W.screencols;
  //int rows = W.screenrows;
  int control_len = strlen(CONTROL_MESSAGE);
  //print control message
  printf("\x1b[2J");
  printf("\x1b[1;%dH", (cols - control_len)/2); 
  printf(CONTROL_MESSAGE);
  printf("\x1b[2;1H");
  fflush(stdout);
}


void printFanSpeed() {
  double fan_speeds[2] = {0};
  int space_len = 10;
  getFanSpeeds(fan_speeds);
  char command[256];
  snprintf(command, sizeof(command), "echo \"%-*d %-*d\" | figlet -f small -w $(tput cols) -c", space_len, (int)fan_speeds[0], space_len, (int)fan_speeds[1]);

  FILE* fp = popen(command, "r");
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
//braille ⠿ 
void drawPlotBorders(int current_row, int max_lines){
  int col_max = W.screencols - 3;
  int col_min = 4;
  printf("\x1b[%d;%dH┌",current_row, col_min);
  printf("\x1b[%d;%dH┐",current_row, col_max);
  printf("\x1b[%d;%dH└",current_row + max_lines, col_min);
  printf("\x1b[%d;%dH┘",current_row + max_lines, col_max);
  for (int i = col_min + 1; i < col_max; i++){
    printf("\x1b[%d;%dH─",current_row, i);
    printf("\x1b[%d;%dH─",current_row + max_lines, i);
  }
 for (int i = 1; i < max_lines; i++) { 
    
    printf("\x1b[%d;%dH│",i + current_row, col_min);
    printf("\x1b[%d;%dH│",i + current_row, col_max);
  }

}
void drawTempPlot(float Temp, int col, int current_row, int max_blocks) {
  int blocks = (int)Temp;
  if (blocks < 10){
    blocks = 10;
  }
  int max_block_height = ((int)(blocks/10))*10 + 10;
  int block_diff = max_block_height - blocks;

  printf("\x1b[%d;%dH%d", current_row + 1, 1, blocks);//prints Temperature next to border
  for (int i = 0; i < max_blocks; i++) { 
    if (i < block_diff){
      printf("\x1b[%d;%dH",i + current_row, col);
      }
     else{
      printf("\x1b[%d;%dH░",i + current_row, col); 
    }
  }
}

/*** refreshes ***/
pthread_mutex_t cursor_mutex;//prevents threads from intefering on cursor movement

void* refreshFanSpeeds(){
  while(1){
    pthread_mutex_lock(&cursor_mutex);
    printf("\x1b[3;1H");//move to third row
    printFanSpeed();
    printf("\x1b[K");
    fflush(stdout);
    pthread_mutex_unlock(&cursor_mutex);
    usleep(600000);
  }
  return NULL;
}

void* refreshPlots(){//needed to modify to work with pthread i.e. void* 
  int cols = W.screencols;
  int col_count_min = 5;
  int padding = col_count_min - 1;
  int col_count_cpu = col_count_min;
  int col_count_gpu = col_count_min;
  char gpu_temp[4];
  int cpu_plot_row = 10;
  int gpu_plot_row = 22;
  int plot_max_lines = 10;
  while (1) {

    pthread_mutex_lock(&cursor_mutex);
    //CPU
    drawPlotBorders(cpu_plot_row, plot_max_lines);
    drawTempPlot(atof(parseNBFCData(TEMP)), col_count_cpu, cpu_plot_row, plot_max_lines); 
    if (col_count_cpu < cols - padding) {
      col_count_cpu++;
    } else {
      col_count_cpu = col_count_min;
      printf("\x1b[%d;%dH",cpu_plot_row, col_count_cpu);
      for (int i = 0; i < 10; i++){
        printf("\x1b[B\x1b[K");
      } 
    }

    //GPU
    getGpuTemp(gpu_temp);
    drawPlotBorders(gpu_plot_row, plot_max_lines);
    drawTempPlot(atof(gpu_temp), col_count_gpu, gpu_plot_row, plot_max_lines); 
    if (col_count_gpu < cols - padding) {
      col_count_gpu++;
    } else {
      col_count_gpu = col_count_min;
      printf("\x1b[%d;%dH",gpu_plot_row, col_count_gpu);
      for (int i = 0; i < 10; i++){
        printf("\x1b[B\x1b[K");
      } 
    }

    fflush(stdout);
    pthread_mutex_unlock(&cursor_mutex);
    usleep(600000);
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
  printHeader();

  pthread_t key_thread, fan_speed_thread, plot_thread;
  pthread_mutex_init(&cursor_mutex, NULL);

  pthread_create(&key_thread, NULL, processKeypress, NULL);
  pthread_create(&plot_thread, NULL, refreshPlots, NULL);
  pthread_create(&fan_speed_thread, NULL, refreshFanSpeeds, NULL);

  pthread_join(key_thread, NULL);
  pthread_join(plot_thread, NULL);
  pthread_join(fan_speed_thread, NULL);
  pthread_mutex_destroy(&cursor_mutex);

 return 0;
}
