#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

const int MAX_RANGE = 800;
const float resolution = 0.1;
const int deafult_sensors = 1;
const int default_sampling = 60, min_sampling=10; //seconds
const int default_interval = 86400, min_interval = 3600; //seconds

int errorLog(int error_code);
int getData(char* data_file, char* argv[], int argc, int* num_sensors, int* sampling_time, int* interval);
int numberCheck(char* input);
void outputData(FILE* dust_sensor, int num_sensors, int sampling_time, int interval);


int main(int argc, char* argv[]) {
    char data_file[] = "dust_sensor.csv";
    FILE* dust_sensor;
    int flag = 0;
    int num_sensors = deafult_sensors;
    int sampling_time = default_sampling;
    int interval = default_interval;

    dust_sensor = fopen(data_file, "w");
    flag = getData(data_file, argv, argc, &num_sensors, &sampling_time, &interval);
    if(flag){
        outputData(dust_sensor, num_sensors, sampling_time, interval);
    }
    fclose(dust_sensor);
    return 0;
}

int errorLog(int error_code){
    switch ((error_code))
    {
    case 1:
        printf("Error 01: invalid command");
        return 0;
    case 2:
        printf("Error 02: invalid argument");
        return 0;;
    case 3:
        printf("Error 03: dust_sensor.csv access denied");
        return 0;
    default:
        return 1;
    }
}

int getData(char* data_file, char* argv[], int argc, int* num_sensors, int* sampling_time, int* interval){
    int state = access(data_file, W_OK);
    int index = 1, value=0;
    if(state == -1){
        errorLog(3);
        return 0;
    }
    if(argc == 1){
        return 1;
    }
    do{
        // check arguments's validation
        if(strcmp(argv[index], "-n") == 0 || strcmp(argv[index], "-st") == 0 || strcmp(argv[index], "-si") == 0){
            if(index+1<argc){
                if(numberCheck(argv[index+1])){
                    value = atoi(argv[index+1]);
                    if(value<=0){
                        errorLog(2);
                        return 0;
                    }
                } else {
                    errorLog(1);
                    return 0;
                }
                if(!strcasecmp(argv[index], "-n")) *num_sensors = value;
                else if(!strcasecmp(argv[index], "-st") && value >= min_sampling) *sampling_time = value;
                else if(!strcasecmp(argv[index], "-si") && value*3600 >= min_interval) *interval = value*3600;
            } else {
                errorLog(1);
                return 0;
            }
        }
        index++;
    } while(index<argc);
    return 1;
}

int numberCheck(char* input){
    int i = 0;
    if (input[0] == '-'){
        i++;
    }
    while(input[i] != '\0'){
        if(isdigit(input[i]) == 0){
            return 0;
        }
        i++;
    }
    return 1;
}

void outputData(FILE* dust_sensor, int num_sensors, int sampling_time, int interval){
    //header of csv file
    fprintf(dust_sensor, "id,time,value\n");

    //init time when run file
    time_t init_time = time(NULL);
    time_t temp;
    struct tm *sim_time;
    char timestamp[20];
    float value;

    for(int step=0; step<=interval; step=step+sampling_time){
        temp = init_time + step;
        sim_time = localtime(&temp);
        strftime(timestamp, sizeof(timestamp), "%Y:%m:%d %H:%M:%S", sim_time);
        for(int sensor=1; sensor<=num_sensors; sensor++){
            value = (rand() % (MAX_RANGE*10+1)) * resolution;
            fprintf(dust_sensor, "%d,%s,%.1f\n", sensor, timestamp, value);
        }
    }
}


