size_t data_buffer_size = 0;
int cpu_data_buffer[1024][2];
int gpu_data_buffer[1024][2];
char temp_buffer[2][4];
char util_buffer[2][4];
void prepareGraphData(){
  getGpuTemp(temp_buffer[1]);
  getUtil(GPU,util_buffer[1]);
  strcpy(temp_buffer[0], parseNBFCData(TEMP));
  getUtil(CPU, util_buffer[0]);
  cpu_data_buffer[data_buffer_size][0] = (int)atof(temp_buffer[0]);
  cpu_data_buffer[data_buffer_size][1] = (int)atof(util_buffer[0]);
  gpu_data_buffer[data_buffer_size][0] = (int)atof(temp_buffer[1]);
  gpu_data_buffer[data_buffer_size][1] = (int)atof(util_buffer[1]);
  data_buffer_size++;
}
void* refreshGraphData(){
  while(1){
    prepareGraphData();

    sleep(COOL_DOWN);
  }
  return NULL;
}
void refreshGraph(int col_count, int temp, int util , int graph_row, int graph_max_lines){
  drawGraphBorders(graph_row, graph_max_lines);
  drawDataGraph(temp, util, col_count, graph_row, graph_max_lines); 
  printCurrentValue(1, temp, graph_row); 
  printCurrentValue(0, util, graph_row); 

  fflush(stdout);
}

void* refreshScreen(){
  int cols = W.screencols;
  int col_count_min = 5;
  int padding = 4;
  int col_count = cols - padding + 1;
  int graph_max_lines = 10;
  int cpu_temp, cpu_util, gpu_temp, gpu_util;
  while(1){
    pthread_mutex_lock(&mutex);
    refreshFanSpeeds();
      cpu_temp = cpu_data_buffer[0][0];
      cpu_util = cpu_data_buffer[0][1]; 
      gpu_temp = gpu_data_buffer[0][0];
      gpu_util = gpu_data_buffer[0][1];
      refreshGraph(cols - 5, cpu_temp, cpu_util, 9, graph_max_lines);
      refreshGraph(cols - 5, gpu_temp, gpu_util, 20, graph_max_lines);
      //clearGraph(cols - padding, 10, col_count, 9);
      //clearGraph(cols - padding, 10, col_count, 20);
    }
    pthread_mutex_unlock(&mutex);
    sleep(COOL_DOWN);

  return NULL;
}
