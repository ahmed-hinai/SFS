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
#include <wchar.h>
#include <locale.h>

/*** dependancies ***/
#define DEPENDANCIES "nbfc-linux, gnuplot" 

/*** defines ***/
#define SFS_VERSION "0.0.1"
#define CTRL_KEY(k) ((k) & 0x1f)
#define CONTROL_MESSAGE  "CTRL+ B: MAX // CTRL+A: AUTO // CTRL+R: RESTART NBFC // CTRL+Q: QUIT"
#define COOL_DOWN 4
#define CPU 0
#define GPU 1
//colors for cli output
#define WHITE "\033[0;39m"
#define WHITE_BG "\033[47m"
#define RED "\033[31m"
#define RED_BG "\033[41m"
#define RED_ON_YELLOW "\033[31;43m"
#define GREEN "\033[32m"
#define YELLOW "\033[33m"
#define BLUE "\033[0;36m"
#define ORANGE "\033[0;33m"
#define ORANGE_BG "\033[43m"
#define YELLOW_ON_RED "\033[33;41m"
#define RESET_BG "\033[0m"
//blocks

wchar_t* A1 = L"\u2594"; // ▔
wchar_t* A2 = L"\U0001FB82"; // 🮂
wchar_t* A3 = L"\U0001FB83"; // 🮃
wchar_t* A4 = L"\u2580"; // ▀
wchar_t* A5 = L"\U0001FB84"; // 🮄
wchar_t* A6 = L"\U0001FB85"; // 🮅
wchar_t* A7 = L"\U0001FB86"; // 🮆

#define B1 "\u2581" // ▁
#define B2 "\u2582" // ▂
#define B3 "\u2583" // ▃
#define B4 "\u2584" // ▄
#define B5 "\u2585" // ▅
#define B6 "\u2586" // ▆
#define B7 "\u2587" // ▇
#define FULL "\u2588" // █

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

/*** globals ***/
int is_service_running = 0;
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

int restartNBFC(){
  system("pkexec /bin/nbfc stop");
  is_service_running = 1;
  system("pkexec /bin/nbfc start");
  is_service_running = 0;
  printf("NBFC RESTARTED");
  char command[64];
  snprintf(command, sizeof(command), "exec %s", "./sfs.o");
  system(command);
  return 0;
}

/*** input ***/
char readKey() {
  int nread;
  char c;
  while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
    if (nread == -1 && errno != EAGAIN) die("read");
  }
  return c;
}


/*** data ***/
char* getNBFCData() {
  

  FILE* fpipe;
  char* command = "/bin/nbfc status";//this can cause fatal Error if nbfc is offline
  char buffer[64];
  char* output = NULL;
  size_t total_size = 0;
  //open pipe
  if (NULL == (fpipe = popen(command, "r"))){
    perror("popen(). failed.");
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
  while(is_service_running == 1){
    sleep(100);
    printf("timed out");
    break;
  }
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

void getUtil(int device, char* util){//CPU: 0, GPU: 1
  char buffer[4];
  FILE* fp;
  switch (device) {
    case 0:
      fp = popen("top -b -n 1 | awk '/^ *%Cpu/ {for(i=1;i<=NF;i++) if($i ~/id/) {print 100 - $(i-1)}}'", "r");
      break;
    case 1:
      fp = popen("nvidia-smi -q | awk '/Utilization/{getline; print $(NF-1)}'", "r");
      break;
  }
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
  int space_len = 8;
  getFanSpeeds(fan_speeds);
  char command[256];
  char* font;
  if (W.screenrows > 26){
    font = "slant";
  } else {
    font = "smslant";
  }
  snprintf(command, sizeof(command), "echo \"%-*d %-*d\" | figlet -f %s -w $(tput cols) -c", space_len, (int)fan_speeds[0], space_len, (int)fan_speeds[1], font);
  FILE* fp = popen(command, "r");
  if (fp == NULL) {
      perror("popen failed");
      return;
  }

  char buffer[256];
  while (fgets(buffer, sizeof(buffer), fp) != NULL) {
      printf("%s", buffer);
    }
  
  pclose(fp);
}
/*
//U+2591 	░ 	Light shade 
//U+2592 	▒ 	Medium shade
//U+2593 	▓ 	Dark shade 
//braille ⠿ 
U+2581 	▁ 	Lower one eighth block
U+2582 	▂ 	Lower one quarter block
U+2583 	▃ 	Lower three eighths block
U+2584 	▄ 	Lower half block
U+2585 	▅ 	Lower five eighths block
U+2586 	▆ 	Lower three quarters block
U+2587 	▇ 	Lower seven eighths block
U+2588 	█ 	Full block 
*/
void drawGraphBorders(int current_row, int max_lines){
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
//
}

void printValueBlock(int i, int last_digit, int current_row, int col){
      switch (last_digit) {
        case 0:
          printf("\x1b[%d;%dH%s",i + current_row, col, FULL); 
          break;
        case 1:
        case 2:
          printf("\x1b[%d;%dH%s",i + current_row, col, B1); 
          break;
        case 3:
        case 4:
          printf("\x1b[%d;%dH%s",i + current_row, col, B2); 
          break;
        case 5:
          printf("\x1b[%d;%dH%s",i + current_row, col, B3); 
          break;
        case 6:
          printf("\x1b[%d;%dH%s",i + current_row, col, B4); 
          break;
        case 7:
          printf("\x1b[%d;%dH%s",i + current_row, col, B5); 
          break;
        case 8:
          printf("\x1b[%d;%dH%s",i + current_row, col, B6); 
          break;
        case 9:
          printf("\x1b[%d;%dH%s",i + current_row, col, B7); 
          break; 
      }
}
void drawDataGraph(float ftemp, float futil, int col, int current_row, int max_lines) {

  int max_value = 100;//temp or util, 100 works as max. thank you celsius!
  int temp = ((int)ftemp > 100) ? 100: (int)ftemp;
  int util = ((int)futil > 100) ? 100: (int)futil;
  
  int temp_diff = max_value - temp;
  int temp_last_digit = temp - ((int)temp/10)*10;
  int util_diff = max_value - util;
  int util_last_digit = util - ((int)util/10)*10;
  
  int smaller_diff;
  int bigger_diff;

  if (temp_diff < util_diff){
    smaller_diff = temp_diff;
    bigger_diff = util_diff;
  } else {
    smaller_diff = util_diff;
    bigger_diff = temp_diff;
  }

  for (int i = 0; i < max_lines; i++) { 
    if (i < (int)smaller_diff/10){
      printf("\x1b[%d;%dH",i + current_row, col);
      }

    else if (i == (int)smaller_diff/10){
      if (smaller_diff == temp_diff){
        printf("%s", RED);
        printValueBlock(i, temp_last_digit, current_row, col);
      } else {
        printf("%s", ORANGE);
        printValueBlock(i, util_last_digit, current_row, col);
      }
    }
    else if (i == (int)bigger_diff/10){
      if (bigger_diff == temp_diff){
        printf("%s", RED_ON_YELLOW); 
        printValueBlock(i, temp_last_digit, current_row, col);
      } else {
        printf("%s", YELLOW_ON_RED);
        printValueBlock(i, util_last_digit, current_row, col);
      }
    }
    else  {
      printf("\x1b[%d;%dH%s", i + current_row, col, FULL);

    }
  }
  printf("%s", RESET_BG);
}
void printCurrentValue(int is_temp, int value, int row){
  if (is_temp){
    printf("\x1b[%d;%dH   ", row + 1, 1);
    printf(RED);
    printf("\x1b[%d;%dH%d", row + 1, 1, value);
    printf(WHITE);
  } else {
    printf("\x1b[%d;%dH   ", row + 2, 1);
    printf(ORANGE);
    printf("\x1b[%d;%dH%d", row + 2, 1, value);
    printf(WHITE);
  }
}
void clearGraph(int cols, int rows, int col_min, int current_row){
    for (int i = col_min; i <= cols; i++){
      for (int k = current_row + 1; k < current_row + rows; k++){
      printf("\x1b[%d;%dH ",k, i);
      fflush(stdout);
    }
  }
}


/*** refreshes ***/
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;//prevents threads from intefering on cursor movement

void* processKeypress(){
  char c;
  while((c = readKey()) != EOF) {
    pthread_mutex_lock(&mutex);
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
        if (restartNBFC() != 0){//solves some bug that prevents restart
        };
        break;

    pthread_mutex_unlock(&mutex);
    }
  }
 return NULL;
}

void refreshFanSpeeds(){
    printf("\x1b[2;1H");//move to third row
    printFanSpeed();
    printf("\x1b[K");
    fflush(stdout);
}

float cpu_temp;
float cpu_util;
float gpu_temp;
float gpu_util;

void prepareGraphData(){
  char cpu_temp_buffer[4];
  char cpu_util_buffer[4];
  char gpu_temp_buffer[4];
  char gpu_util_buffer[4];
  getGpuTemp(gpu_temp_buffer);
  getUtil(GPU, gpu_util_buffer);
  strcpy(cpu_temp_buffer, parseNBFCData(TEMP));
  getUtil(CPU, cpu_util_buffer);
  cpu_temp = atof(cpu_temp_buffer);
  cpu_util = atof(cpu_util_buffer);
  gpu_temp = atof(gpu_temp_buffer);
  gpu_util = atof(gpu_util_buffer);
}

void* refreshGraphData(){
  while(1){
    prepareGraphData();
    sleep(COOL_DOWN);
  }
  return NULL;
}
void refreshGraph(int col_count, int device, int graph_row, int graph_max_lines){
  drawGraphBorders(graph_row, graph_max_lines);
  if (device){
    drawDataGraph(gpu_temp, gpu_util, col_count, graph_row, graph_max_lines); 
    printCurrentValue(1, gpu_temp, graph_row); 
    printCurrentValue(0, gpu_util, graph_row); 
  } else {
    drawDataGraph(cpu_temp, cpu_util, col_count, graph_row, graph_max_lines); 
    printCurrentValue(1, cpu_temp, graph_row); 
    printCurrentValue(0, cpu_util, graph_row); 
  }

  fflush(stdout);
}

void* refreshScreen(){
  int cols = W.screencols;
  int col_count = 4;
  int padding = 4;
  int graph_max_lines = 10;
  while(1){
    pthread_mutex_lock(&mutex);
    refreshFanSpeeds();
    refreshGraph(col_count, CPU, 9, graph_max_lines);
    refreshGraph(col_count, GPU, 20, graph_max_lines);
    if (col_count < cols - padding){
      col_count++;
    }
    else {
      col_count = 4;
      clearGraph(cols - padding, 10, 5, 9);
      clearGraph(cols - padding, 10, 5, 20);
    }
    pthread_mutex_unlock(&mutex);
    sleep(COOL_DOWN);
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

  pthread_t key_thread, data_prep_thread, print_thread; 

  pthread_create(&key_thread, NULL, processKeypress, NULL);
  pthread_create(&data_prep_thread, NULL, refreshGraphData, NULL);
  pthread_create(&print_thread, NULL, refreshScreen, NULL);

  pthread_join(key_thread, NULL);
  pthread_join(data_prep_thread, NULL);
  pthread_join(print_thread, NULL);

 return 0;
}
