#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <time.h>

#define STOP 0
#define CONTINUE_WITH_ERROR 1
#define NO_ERROR 2


FILE* task_log;//file to log error and warning

int errorLog(int error_code, int missing, int dup1, int dup2);//return 0 if error, 1 if continue with error, 2 if no error
int commandCheck(int argc, char* argv[]);//return 1 if valid, 0 if invalid
int validationCheck(FILE* dust_sensor, FILE* dust_outlier, FILE* dust_valid);//return number of sensors and 2 file if input is valid, 0 if invalid
int checkID(char *id);//return 1 if valid, 0 if invalid
int checkTime(char *time);//return 1 if valid, 0 if invalid
int checkValue(char *value);//return 1 if valid, 0 if invalid
int checkDuplicate(FILE* dust_valid, char* data[10], int line_num);//return 1 if no duplicate, 0 if duplicate
void AQIProcess(FILE* dust_valid, FILE* dust_aqi, int sensor_count);//write to fild dust_aqi
char *poluteCode(float concentration);//return polution code
time_t timeConvert(char *data);
int concentrationToAQI(float concentration);
void Analysis(FILE* dust_valid, FILE* dust_aqi, FILE* dust_summary, FILE* dust_statistics, int sensor_count);
int Comparator(const void* a, const void* b);

int main(int argc, char* argv[]) {
    int sensor_count = 0;
    FILE* dust_sensor;
    FILE* dust_outlier;
    FILE* dust_valid;
    FILE* dust_aqi;
    FILE* dust_summary;
    FILE* dust_statistics;
    task_log = fopen("task2.log", "w");
    int file_valid = 0;
    file_valid = commandCheck(argc, argv);

    if(file_valid) {
        dust_sensor = fopen(argv[1], "r");
        dust_outlier = fopen("dust_outlier.csv", "w");
        dust_valid = fopen("dust_valid.csv", "w+");
        dust_aqi = fopen("dust_aqi.csv", "w+");
        dust_summary = fopen("dust_summary.csv", "w");
        dust_statistics = fopen("dust_statistics.csv", "w");

        sensor_count = validationCheck(dust_sensor, dust_outlier, dust_valid);

        rewind(dust_valid);
        AQIProcess(dust_valid, dust_aqi, sensor_count);

        Analysis(dust_valid, dust_aqi, dust_summary, dust_statistics, sensor_count);

        fclose(dust_sensor);
        fclose(dust_outlier);
        fclose(dust_valid);
        fclose(dust_aqi);
        fclose(dust_statistics);
    }
    fclose(task_log);
    return 0;
}

void Analysis(FILE* dust_valid, FILE* dust_aqi, FILE* dust_summary, FILE* dust_statistics, int sensor_count){
    const int len = 1e4/sensor_count;
    const char header_summary[] = "id,parameter,time,value";
    const char header_statistics[] = "id,polution,duration";
    char* token;
    char line[256];
    char *data[3];
    time_t init = (time_t) 0;
    time_t final = (time_t) 0;
    long long int diff = 0;
    time_t max_time = (time_t) 0;
    time_t min_time = (time_t) 0;
    struct tm *temp;
    int hours, minutes, seconds;
    char t_max[20], t_min[20];
    int id = 0;
    int count = 0;
    double mean = 0, median = 0;
    double sen_value[len][2]; //every value of each sensor, use to calculate median and mean
    int G[sensor_count]; //good
    int M[sensor_count]; //moderate
    int SU[sensor_count]; //slightly unhealthy
    int U[sensor_count]; //unhealthy
    int VU[sensor_count]; //very unhealthy
    int H[sensor_count]; //hazardous
    int EH[sensor_count]; //extrememly hazardous

    for(int i=0; i<sensor_count; i++){
        G[i] = 0;
        M[i] = 0;
        SU[i] = 0;
        U[i] = 0;
        VU[i] = 0;
        H[i] = 0;
        EH[i] = 0;
    }
    
    for(int i=0; i<len; i++){
        sen_value[i][0] = 0;
        sen_value[i][1] = 0;
    }
    
    //summary file
    fprintf(dust_summary, "%s\n", header_summary);
    for(int id_count=0; id_count<sensor_count; id_count++){
        //back to start of file
        rewind(dust_valid);
        //skip header line
        fgets(line, sizeof(line), dust_valid);
        //reset value
        id = id_count+1;
        count = 0;
        mean = 0; median = 0;
        while(fgets(line, sizeof(line), dust_valid) != NULL){
            token = strtok(line, ",");
            data[0] = token;
            token = strtok(NULL, ",");
            data[1] = token;
            token = strtok(NULL, ",");
            data[2] = token;
            if(id == atoi(data[0])){
                sen_value[count][0] = atof(data[2]); // Copy value of data[1] into sen_value[count]
                sen_value[count][1] = (double) timeConvert(data[1]);
                count++;
            }
        }
        init = (time_t)sen_value[0][1];
        final = (time_t)sen_value[count-1][1];
        diff = difftime(final, init);
        hours = (int)diff/3600;
        minutes = (int)((diff%3600)/60);
        seconds = (int)(diff%60);


        qsort(sen_value, count, sizeof(sen_value[0]), Comparator);
        max_time = (time_t)sen_value[count-1][1];
        min_time = (time_t)sen_value[0][1];

        temp = localtime(&max_time);
        strftime(t_max, sizeof(t_max), "%Y:%m:%d %H:%M:%S", temp);
        temp = localtime(&min_time);
        strftime(t_min, sizeof(t_min), "%Y:%m:%d %H:%M:%S", temp);


        for(int i = 0; i < count; i++){
            mean = mean + sen_value[i][0];
        }
        mean = mean/count;
        if(count%2 == 1){
            median = sen_value[count/2 + 1][0];
        } else {
            median = sen_value[count/2][0];
        }
        fprintf(dust_summary, "%d,max,%s,%.1lf\n", id, t_max, sen_value[count-1][0]);
        fprintf(dust_summary, "%d,min,%s,%.1lf\n", id, t_min, sen_value[0][0]);
        fprintf(dust_summary, "%d,mean,%d:%d:%d,%.1lf\n", id, hours, minutes, seconds, mean);
        fprintf(dust_summary, "%d,median,%d:%d:%d,%.1lf\n", id, hours, minutes, seconds, median);
    }

    //statistics file
    fprintf(dust_statistics, "%s\n", header_statistics);
    

    //back to start of file
    rewind(dust_aqi);
    //skip 1st line of file
    fgets(line, sizeof(line),dust_aqi);
    while(fgets(line,sizeof(line), dust_aqi) != NULL){
        token = strtok(line, ",");
        data[0] = token;
        token = strtok(NULL, ",");
        token = strtok(NULL, ",");
        token = strtok(NULL, ",");
        token = strtok(NULL, ",");
        data[1] = token;
        if(strcmp(data[1], "Good\n") == 0) G[atoi(data[0])-1]++;
        if(strcmp(data[1], "Moderate\n") == 0) M[atoi(data[0])-1]++;
        if(strcmp(data[1], "Slightly Unhealthy\n") == 0) SU[atoi(data[0])-1]++;
        if(strcmp(data[1], "Unhealthy\n") == 0) U[atoi(data[0])-1]++;
        if(strcmp(data[1], "Very Unhealthy\n") == 0) VU[atoi(data[0])-1]++;
        if(strcmp(data[1], "Hazardous\n") == 0) H[atoi(data[0])-1]++;
        if(strcmp(data[1], "Extremely Hazardous\n") == 0) EH[atoi(data[0])-1]++;
    }
    for(int senc=0; senc<sensor_count; senc++){
        id = senc+1;
        fprintf(dust_statistics, "%d,Good,%d\n", id, G[senc]);
        fprintf(dust_statistics, "%d,Moderate,%d\n", id, M[senc]);
        fprintf(dust_statistics, "%d,Slightly Unhealthy,%d\n", id, SU[senc]);
        fprintf(dust_statistics, "%d,Unhealthy,%d\n", id, U[senc]);
        fprintf(dust_statistics, "%d,Very Unhealthy,%d\n", id, VU[senc]);
        fprintf(dust_statistics, "%d,Hazardous,%d\n", id, H[senc]);
        fprintf(dust_statistics, "%d,Extremely Hazardous,%d\n", id, EH[senc]);
    }
}

int Comparator(const void* a, const void* b){
    const double *row_a = (const double*)a;
    const double *row_b = (const double*)b;

    if (row_a[0] < row_b[0]) return -1;
    if (row_a[0] > row_b[0]) return 1;
    return 0;
}

void AQIProcess(FILE* dust_valid, FILE* dust_aqi, int sensor_count){
    const char header[256] = "id,time,value,AQI,polution";
    char line[256];
    char* token;
    time_t init[sensor_count];
    time_t final[sensor_count];
    time_t temp_time = 0;
    int id;
    char time[20];//store time of rec
    char input_time[20];//temporary variable to extract time_t init and final
    float temp_val = 0;
    float value[sensor_count]; //total value of measurement
    int count[sensor_count]; //number of measurement
    int AQI = 0;
    char *state;

    //initialize intit time and final time and counting
    for(int i=0; i<sensor_count; i++){
        init[i] = (time_t) 0;
        final[i] = (time_t) 0;
        count[i] = 0;
        value[i] = 0;
    }

    //write header for dust_aqi
    fprintf(dust_aqi, "%s\n", header);

    //find avergae AQI per hour for sensor each sensors
    fgets(line, sizeof(line), dust_valid); //skip first line
    while(fgets(line, sizeof(line), dust_valid) != NULL) {
        token = strtok(line, ",");
        id = atoi(token);
        token = strtok(NULL, ",");
        strcpy(time, token);
        token = strtok(NULL, ",");
        temp_val = atof(token);

        strcpy(input_time, time);
        temp_time = timeConvert(input_time);
        if(init[id-1] == 0){
            init[id-1] = temp_time;
        } else {
            final[id-1] = temp_time;
        }
        count[id-1]++;
        value[id-1] = value[id-1] + temp_val;
        if(final[id-1] - init[id-1] >= 3600){
            value[id-1] = value[id-1]/count[id-1];
            AQI = concentrationToAQI(value[id-1]);
            state = poluteCode(value[id-1]);
            fprintf(dust_aqi, "%d,%s,%.1f,%d,%s\n", id, time, value[id-1], AQI, state);
            init[id-1] = (time_t) 0;
            final[id-1] = (time_t) 0;
            count[id-1] = 0;
            value[id-1] = 0;
            free(state);
        }
    }


}

int concentrationToAQI(float concentration) {
    if (concentration >= 0 && concentration < 12) {
        return (int)(concentration * 50 / 12);
    } else if (concentration >= 12 && concentration < 35.5) {
        return (int)(50 + (concentration - 12) * 50 / (35.5 - 12));
    } else if (concentration >= 35.5 && concentration < 55.5) {
        return (int)(100 + (concentration - 35.5) * 50 / (55.5 - 35.5));
    } else if (concentration >= 55.5 && concentration < 150.5) {
        return (int)(150 + (concentration - 55.5) * 50 / (150.5 - 55.5));
    } else if (concentration >= 150.5 && concentration < 250.5) {
        return (int)(200 + (concentration - 150.5) * 100 / (250.5 - 150.5));
    } else if (concentration >= 250.5 && concentration < 350.5) {
        return (int)(300 + (concentration - 250.5) * 100 / (350.5 - 250.5));
    } else if (concentration >= 350.5 && concentration <= 550.5) {
        return (int)(400 + (concentration - 350.5) * 100 / (550.5 - 350.5));
    } else {
        return -1; // Invalid concentration
    }
}

time_t timeConvert(char *data){
    const int init_year = 1900;
    time_t res;
    struct tm time = {0};
    char *token = NULL;
    char *first_split[2];
    char *second_split[6];
    int count = 0;

    token = strtok(data, " ");
    first_split[0] = token;
    token = strtok(NULL, " ");
    first_split[1] = token;

    token = strtok(first_split[0], ":");
    while (token != NULL)
    {
        second_split[count] = token;
        token = strtok(NULL, ":");
        count++;
    }

    token = strtok(first_split[1], ":");
    while (token != NULL)
    {
        second_split[count] = token;
        token = strtok(NULL, ":");
        count++;
    } 

    time.tm_isdst = -1;
    time.tm_sec = atoi(second_split[5]);
    time.tm_min = atoi(second_split[4]);
    time.tm_hour = atoi(second_split[3]);
    time.tm_mday = atoi(second_split[2]);
    time.tm_mon = atoi(second_split[1]) - 1; // tm_mon is 0-based
    time.tm_year = atoi(second_split[0]) - init_year;
    
    res = mktime(&time);

    return res;
}

char *poluteCode(float concentration){
    char* G = (char* )malloc(5*sizeof(char)); strcpy(G, "Good");
    char* M = (char* )malloc(9*sizeof(char)); strcpy(M, "Moderate");
    char* SU = (char* )malloc(19*sizeof(char)); strcpy(SU, "Slightly Unhealthy");
    char* U = (char* )malloc(10*sizeof(char)); strcpy(U, "Unhealthy");
    char* VU = (char* )malloc(15*sizeof(char)); strcpy(VU, "Very Unhealthy");
    char* H = (char* )malloc(10*sizeof(char)); strcpy(H, "Hazardous");
    char* EH = (char* )malloc(21*sizeof(char)); strcpy(EH, "Extremely Hazardous");
    char* I = (char* )malloc(8*sizeof(char)); strcpy(I, "Invalid");
    if(concentration >= 0 && concentration < 12) return G;
    if(concentration >= 12 && concentration < 35.5) return M;
    if(concentration >= 35.5 && concentration < 55.5) return SU;
    if(concentration >= 55.5 && concentration < 150.5) return U;
    if(concentration >= 150.5 && concentration < 250.5) return VU;
    if(concentration >= 250.5 && concentration < 350.5) return H;
    if(concentration >= 350.5 && concentration <= 500.5) return EH;
    return I;
}

int errorLog(int error_code, int missing, int dup1, int dup2){
    switch ((error_code))
    {
    case 1:
        fprintf(task_log, "Error 01: input file not found or not accessible\n");
        return STOP;
    case 2:
        fprintf(task_log, "Error 02: invalid csv file format\n");
        return STOP;
    case 3:
        fprintf(task_log, "Error 03: invalid command\n");
        return STOP;
    case 4:
        fprintf(task_log, "Error 04: data is missing at line %d\n", missing);
        return CONTINUE_WITH_ERROR;
    case 5:
        fprintf(task_log, "Error 05: data is duplicated at line %d and %d\n", dup1, dup2);
        return CONTINUE_WITH_ERROR;
    case 6:
        fprintf(task_log, "Error 06: input file is too large\n");
        return CONTINUE_WITH_ERROR;
    default:
        return NO_ERROR;
    }
}

int commandCheck(int argc, char* argv[]){
    if(argc < 2) {errorLog(3, -1, -1, -1); return 0;}
    if(access(argv[1], R_OK) == -1) {errorLog(1, -1, -1, -1); return 0;}
    return 1;
}

int validationCheck(FILE* dust_sensor, FILE* dust_outlier, FILE* dust_valid){
    const char header[256] = "id,time,value\n"; //proper header format
    int size = -1; //to check empty file
    char line[256];//to read line by line
    char* token = NULL;//to split line
    char* data[10];//get each element in a line
    int count = 0;//count number of elements in a line
    int line_num = 0; //current line number
    FILE* temp_outlier = tmpfile(); //temporary file to store outlier data
    FILE* temp_valid = tmpfile(); //temporary file to store valid data
    int outlier_count = 0; //count number of outlier data
    long current_pos = 0; //current position of file pointer
    int sensor_count=0; //count number of sensors

    // check if dust_sensor is empty
    fseek(dust_sensor, 0, SEEK_END);
    size = ftell(dust_sensor);
    if(size == 0) {
        errorLog(2, -1, -1, -1);
        return 0;
    }
    fseek(dust_sensor, 0, SEEK_SET);

    // header check
    fgets(line, sizeof(line), dust_sensor);
    if(strcmp(line, header) != 0) {
        errorLog(2, -1, -1, -1);
        return 0;
    }

    //get data, check for missing data, data formated, duplicated data, and write to dust_valid
    while (fgets(line, sizeof(line), dust_sensor) != NULL)
    {
        line_num ++;
        if(line_num>10000) {
            errorLog(6, -1, -1, -1);
            break;
        }
        //get data from line and check for size
        token = strtok(line, ",");
        while (token != NULL)
        {
            data[count] = token;
            token = strtok(NULL, ",");
            count++;
        }
        if(count != 3) {
            errorLog(2, -1, -1, -1);
            count = 0;
            return 0;
        }

        //check id, timestamp and value format
        if(checkID(data[0]) == 0 || checkTime(data[1]) == 0 || checkValue(data[2]) == 0) {
            errorLog(4, line_num, -1, -1);
            count = 0;
            continue;
        }

        //check for duplicate data
        current_pos = ftell(dust_sensor);
        if(!checkDuplicate(dust_sensor, data, line_num)){
            fseek(dust_sensor, current_pos, SEEK_SET);
            count = 0;
            continue;
        }

        //write to temp file of dust_outlier and dust_valid
        if(atof(data[2])<3 || atof(data[2])>550.5) {
            fprintf(temp_outlier, "%s,%s,%s", data[0], data[1], data[2]);
            outlier_count++;
        } else{
            fprintf(temp_valid, "%s,%s,%s", data[0], data[1], data[2]);
            if(atoi(data[0])>sensor_count) sensor_count = atoi(data[0]);
        }
        count = 0;
    }

    //write final dust_outlier
    rewind(temp_outlier);
    fprintf(dust_outlier, "number of outliers: %d\n", outlier_count);
    fprintf(dust_outlier, "%s",header);
    while (fgets(line, sizeof(line), temp_outlier) != NULL)
    {
        fprintf(dust_outlier, "%s", line);
    }
    fclose(temp_outlier);

    //write to final dust_valid
    rewind(temp_valid);
    fprintf(dust_valid, "%s", header);
    while (fgets(line, sizeof(line), temp_valid) != NULL)
    {
        fprintf(dust_valid, "%s", line);
    }    
    fclose(temp_valid);
    return sensor_count;
}

int checkDuplicate(FILE* dust_sensor, char* data[10], int line_num){
    char line[256];
    char* token = NULL;
    char* dup_data[10];
    int dup_line = 0;

    //rewind file pointer to the beginning
    rewind(dust_sensor);
    //skip header
    fgets(line, sizeof(line), dust_sensor);
    
    while (fgets(line, sizeof(line), dust_sensor) != NULL){
        dup_line++;
        if(dup_line == line_num) return 1;
        token = strtok(line, ",");
        dup_data[0] = token;
        token = strtok(NULL, ",");
        dup_data[1] = token;
        if(strcmp(data[0], dup_data[0]) == 0 && strcmp(data[1], dup_data[1]) == 0){
            errorLog(5, -1, line_num, dup_line);
            return 0;
        }
    }

    return 1;
}

int checkID(char *id){
    if(id == NULL || *id == '\0') return 0;

    for(int i = 0; id[i] != '\0'; i++){
        if(!isdigit(id[i])) return 0;
    }

    if(atoi(id) <= 0) return 0;

    return 1;
}

int checkTime(char *timestamp) {
    // Check if the length is exactly 19 characters
    if (strlen(timestamp) != 19) {
        return 0;
    }

    // Check each character to ensure it matches the expected format
    for (int i = 0; i < 19; i++) {
        if (i == 4 || i == 7) {
            if (timestamp[i] != ':') {
                return 0;
            }
        } else if (i == 10) {
            if (timestamp[i] != ' ') {
                return 0;
            }
        } else if (i == 13 || i == 16) {
            if (timestamp[i] != ':') {
                return 0;
            }
        } else {
            if (!isdigit(timestamp[i])) {
                return 0;
            }
        }
    }

    return 1;
}

int checkValue(char *value){
    int index=0;
    int count = 0;
    char tmp = '.';
    if(value == NULL || *value == '\0' || strlen(value) < 2) return 0;
    if(value[0] == '-') index = 1;

    for(int i = index; value[i] != '\0'; i++){
        if(!isdigit(value[i]) && value[i] != '.' && value[i] != '\n') return 0;
        if(value[i] == '.' ) count++;
    }
   
    if(count > 1) return 0;

    return 1;
}






