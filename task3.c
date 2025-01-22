#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

#define START_BYTE 0x0A
#define STOP_BYTE 0x0F

void log_error(const char *message)
{
    FILE *log_file = fopen("task3.log", "a");
    if(log_file)
    {
        fprintf(log_file, "%s\n", message);
        fclose(log_file);
    }
}

typedef struct {
    uint8_t start_byte;
    uint8_t packet_length;
    uint8_t id;
    uint32_t time;
    float speed;
    float acceleration;
    uint8_t checksum;
    uint8_t stop_byte;
} DataPacket;

uint32_t convert_to_unix_timestamp(const char *datetime) {
    struct tm t;
    memset(&t, 0, sizeof(struct tm));
    if (sscanf(datetime, "%d:%d:%d %d:%d:%d", 
           &t.tm_year, &t.tm_mon, &t.tm_mday, 
           &t.tm_hour, &t.tm_min, &t.tm_sec) != 6) {
        log_error("Error 02: invalid input file format");
        return 0; // Return 0 to indicate error
    }
    t.tm_year -= 1900; // Year since 1900
    t.tm_mon -= 1;     // Month in range 0-11
    t.tm_isdst = -1;   // Not set by sscanf; tells mktime to determine if DST is in effect
    return mktime(&t);
}

uint8_t calculate_checksum(DataPacket packet) {
    uint8_t checksum = packet.packet_length + packet.id +
                       ((uint8_t*)&packet.time)[0] + ((uint8_t*)&packet.time)[1] +
                       ((uint8_t*)&packet.time)[2] + ((uint8_t*)&packet.time)[3] +
                       ((uint8_t*)&packet.speed)[0] + ((uint8_t*)&packet.speed)[1] +
                       ((uint8_t*)&packet.speed)[2] + ((uint8_t*)&packet.speed)[3] +
                       ((uint8_t*)&packet.acceleration)[0] + ((uint8_t*)&packet.acceleration)[1] +
                       ((uint8_t*)&packet.acceleration)[2] + ((uint8_t*)&packet.acceleration)[3];
    return ~checksum + 1; // Two's complement
}

void convert_to_packet(char *line, DataPacket *packet, float prev_speed, uint32_t prev_time, int *prev_id, float *prev_prev_speed, uint32_t *prev_prev_time, int line_number) {
    int id;
    char datetime[100000];
    float speed;

    if (sscanf(line, "%d,%19[^,],%f", &id, datetime, &speed) != 3) {
        log_error("Error 02: invalid input file format");
        return;
    }

    if (id < 0 || id > 255) {
        fprintf(stderr, "Error 04: invalid data at line %d\n", line_number);
        log_error("Error 04: invalid data");
        return;
    }

    uint32_t timestamp = convert_to_unix_timestamp(datetime);
    if (timestamp == 0) {
        fprintf(stderr, "Error 04: invalid data at line %d\n", line_number);
        log_error("Error 04: invalid data");
        return;
    }

    if (speed <= 0) {
        fprintf(stderr, "Error 04: invalid data at line %d\n", line_number);
        log_error("04: invalid data");
        return;
    }

    if (*prev_id == id && *prev_prev_speed == prev_speed && *prev_prev_time == prev_time) {
        fprintf(stderr, "Error 05: data at line %d and %d are duplicated\n", line_number - 1, line_number);
        log_error("Error 05: data is duplicated");
        return;
    }

    packet->start_byte = START_BYTE;
    packet->packet_length = sizeof(DataPacket);
    packet->id = (uint8_t)id;
    packet->time = timestamp;
    packet->speed = speed;
    packet->acceleration = prev_time ? (speed - prev_speed) / (packet->time - prev_time) : 0.0;
    packet->checksum = calculate_checksum(*packet);
    packet->stop_byte = STOP_BYTE;

    // Update previous data
    *prev_id = id;
    *prev_prev_speed = prev_speed;
    *prev_prev_time = prev_time;
}

void write_packet(FILE *output_file, DataPacket packet) {
    // Write each byte of the DataPacket in hexadecimal format
    uint8_t *data = (uint8_t*)&packet;
    // Print exactly 16 bytes per line, excluding bytes to skip
    for (size_t i = 0; i < sizeof(DataPacket) - 2; i++) {
        fprintf(output_file, "%02X ", data[i]);
    }
    fprintf(output_file, "%02X\n", data[sizeof(DataPacket) - 2]);
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <input_file> <output_file>\n", argv[0]);
        return 1;
    }

    char *input_filename = argv[1];
    char *output_filename = argv[2];

    FILE *input_file = fopen(input_filename, "r");
    if (!input_file) {
        fprintf(stderr, "Error 01: input file not found or not accessible\n");
        log_error("Error 01: input file not found or not accessible");
        return 1;
    }

    FILE *output_file = fopen(output_filename, "w");
    if (!output_file) {
        fprintf(stderr, "Error 07: cannot override output file\n");
        log_error("Error 07: cannot override the hex file");
        fclose(input_file);
        return 1;
    }

    char line[100000];
    fgets(line, sizeof(line), input_file); // Skip header

    float prev_speed = 0;
    uint32_t prev_time = 0;
    int prev_id = -1;
    float prev_prev_speed = 0;
    uint32_t prev_prev_time = 0;
    int line_number = 1; // Line number counter

    while (fgets(line, sizeof(line), input_file)) {
        DataPacket packet;
        convert_to_packet(line, &packet, prev_speed, prev_time, &prev_id, &prev_prev_speed, &prev_prev_time, line_number);
        write_packet(output_file, packet);

        prev_speed = packet.speed;
        prev_time = packet.time;
        line_number++;
    }

    fclose(input_file);
    fclose(output_file);

    return 0;
}
