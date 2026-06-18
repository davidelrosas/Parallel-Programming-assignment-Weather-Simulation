#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>
#include <omp.h>

#define MAX_CHAR 1024
#define LIST_SIZE 525700 // Ammount of minutes in a year (length of simulation) (I put a little bit more just in case)
int num_stations;

typedef struct Messstation Messstation;
struct Messstation
{
    char station_id[128];
    float local_temp_change;
    float temp_change_adjusted;
    float measurements[LIST_SIZE];

    Messstation *North;
    Messstation *West;
    Messstation *South;
    Messstation *East;
};

//*****************DATA INITIALIZATION FUNCTIONS****************

// function to count files inside directory
int count_files(char *dir_path)
{
    DIR *dir = opendir(dir_path);
    int count = 0;
    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL)
    {
        // Keep in mind there could be potential issues because it checks if the string contains .csv but not if it strictly ends with .csv
        if (strstr(entry->d_name, ".csv"))
            count++;
    }

    closedir(dir);
    return count;
}

// reads the entry at a specific row and at a specific column, First Column must be UNIQUE ID
char *read_row_col(const char *row_id, char *column, char *file_path)
{
    FILE *fp = fopen(file_path, "r");
    if (fp == NULL)
    {
        perror("Error opening file, please enter a valid file path");
        exit(EXIT_FAILURE);
    }

    int col_pos = 0;
    char line[MAX_CHAR];
    char *token;
    fgets(line, MAX_CHAR, fp);

    // retrieve position of column
    token = strtok(line, ",");
    while (token != NULL)
    {
        if (strstr(token, column))
            break;
        token = strtok(NULL, ",");
        col_pos++;
    }

    // finding row with id
    while (feof(fp) != true)
    {
        fgets(line, MAX_CHAR, fp);
        token = strtok(line, ",");

        if (strstr(token, row_id))
        {
            // retrieving token from row(row_id)
            for (int i = 0; i < col_pos; i++)
                token = strtok(NULL, ",");
            return token;
        }
    }

    fclose(fp);
    // returning the token at row_id and column
    return "N/A";
}

// SETS DATA OF MESSTATIONEN, EXCEPT NEIGHBOURS WHICH WILL BE DONE IN A SEPARATE FUNCTION
Messstation *set_station_data(char *file_path)
{
    FILE *fp = fopen(file_path, "r");
    char row[MAX_CHAR];
    char *token;
    Messstation *station = malloc(sizeof(Messstation));

    if (fp == NULL)
    {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    // Set station name
    fgets(row, MAX_CHAR, fp);
    token = strtok(row, "# :\n");
    token = strtok(NULL, "# :\n");

    // token cannot exceed the size of 128 characters
    strcpy(station->station_id, token);

    printf("setting station name to: %s\n", station->station_id);

    // reading from which channel to read the temperature (with a csv file we made)
    // reading temp channel for station
    char *temp_channel = read_row_col(station->station_id, "channel", "gitter.csv");
    printf("reading temp from channel: %s\n", temp_channel);

    while (strstr(row, "# Sensoren in dieser Tabelle") == NULL)
        fgets(row, MAX_CHAR, fp);

    // counting where the column for this channel is in the csv file (starting at column 0)
    int temp_col_placement = 7;
    while (strstr(row, "# Datensektion") == NULL)
    {
        if (temp_channel == "N/A")
            break;

        fgets(row, MAX_CHAR, fp);
        temp_col_placement++;
        if (strstr(row, temp_channel) != NULL)
            break;

        // incremennt because for each meassurement there is an extra Quality column
        temp_col_placement++;
    }
    printf("temp column placement is %d\n\n", temp_col_placement);

    // jumping to where the data begins
    while (strstr(row, "# begin of data") == NULL)
        fgets(row, MAX_CHAR, fp);

    int i = 0;
    float temp;

    // PLACING TEMPREATURE VALUES INTO MESSTATION DATASTRUCURE
    while (strstr(row, "# end of data") == NULL)
    {
        fgets(row, MAX_CHAR, fp);
        token = strtok(row, "\t");
        for (int i = 0; i < temp_col_placement; i++)
            token = strtok(NULL, "\t");

        if (token == NULL)
        {
            // What do we place inside the array?
            temp = NAN;
            station->measurements[i] = temp;
            continue;
        }
        else
        {
            temp = strtof(token, NULL);
            station->measurements[i] = temp;
        }
        i++;
    }

    fclose(fp);
    printf("finished setting data for %s \n\n", station->station_id);
    return station;
}

// Function to initiate DATA, also sets num_stations
Messstation **init_data(void)
{

    char *dir_path = "MEVIS_DATEN";
    Messstation **station_list = malloc(num_stations * sizeof(Messstation *));

    // open directory
    struct dirent *entry;
    DIR *dir = opendir(dir_path);

    if (dir == NULL)
    {
        perror("Error opening Directory");
        exit(EXIT_FAILURE);
    }

    printf("The number of weather stations is: %d \n", num_stations);
    printf("please wait, the data is being loaded...\n\n");

    // Parse Through Directory

    int i = 0;
    while ((entry = readdir(dir)) != NULL)
    {
        if (strstr(entry->d_name, ".csv"))
        {
            char file_path[1024];
            snprintf(file_path, sizeof(file_path), "%s/%s", dir_path, entry->d_name);
            printf("load data from: %s\n", file_path);

            // eventually free each i inside of station_list and then station_list itself
            station_list[i] = set_station_data(file_path);
            i++;
        }
    }

    closedir(dir);
    printf("The data has been succesfully loaded\n\n\n");
    return station_list;
}

// this function finds the pointer to a station by name
// There is a problem at THIS FUNCTION NOW
Messstation *find_station(Messstation **station_list, const char *station_name)
{
    // can we somehow count the size of station_list? maybe counting the bytes until we get to a null pointer?
    for (int i = 0; i < num_stations; i++)
    {
        if (strstr(station_list[i]->station_id, station_name) != NULL)
        {
            return station_list[i];
        }
    }
    return NULL;
}

// This function connects the station structs to their respective nighbours with help of the gitter.csv file
void find_neighbours(Messstation **station_list)
{
    char *file_path = "gitter.csv";

    char *token;
    // using num_stations without declaring it inside the function seems a bit weird.
    for (int i = 0; i < num_stations; i++)
    {

        token = read_row_col(station_list[i]->station_id, "north", file_path);
        station_list[i]->North = find_station(station_list, token);

        token = read_row_col(station_list[i]->station_id, "west", file_path);
        station_list[i]->West = find_station(station_list, token);

        token = read_row_col(station_list[i]->station_id, "south", file_path);
        station_list[i]->South = find_station(station_list, token);

        token = read_row_col(station_list[i]->station_id, "east", file_path);
        station_list[i]->East = find_station(station_list, token);

        printf("The neighbours of %s are:\n", station_list[i]->station_id);
        printf("North: %s\n", station_list[i]->North->station_id);
        printf("West: %s\n", station_list[i]->West->station_id);
        printf("South: %s\n", station_list[i]->South->station_id);
        printf("East: %s\n", station_list[i]->East->station_id);
        printf("\n");
    }

    printf("sucessfully loaded geographical data\n");
    return;
}

// just to visualize that the for loop is working
void progress_bar(int total_it, int current_it)
{
    float percentage = (float)current_it / total_it;
    int progress = (int)(percentage * 100);

    printf("%3d %%\r", progress);
    fflush(stdout);
    return;
}

//**********OUTPUT FUNCTIONS*********************************
// adds a token to the line and separates by comma adds new line character if new_line =true
void add_token(const char *token, bool new_line, FILE *fp)
{
    int ret_code;
    ret_code = fwrite(token, sizeof(char), strlen(token), fp);
    ret_code = fwrite(",", 1, 1, fp);
    if (new_line)
        ret_code = fwrite("\n", 1, 1, fp);

    return;
}

// function to create output file where results will be written
FILE *create_output(Messstation **station_list)
{
    FILE *fp = fopen("output_2.csv", "w");

    if (fp == NULL)
    {
        perror("Error: file could not be created");
        exit(EXIT_FAILURE);
    }

    char *header = "#MINUTE, OF THE YEAR,----------------,---------ADJUSTED, TEMPERATURE, CHANGE, BY STATION,------------------,----------------,-------------\n\n\n";
    int ret_code;
    ret_code = fwrite(header, sizeof(char), strlen(header), fp);

    if (ret_code != strlen(header))
    {
        perror("Error writing to output file\n");
        exit(EXIT_FAILURE);
    }

    // Create Columns day/hour/minute and for all the station identifiers

    int num_headers = num_stations + 3;
    const char *headers[num_headers];

    for (int i = 0; i < num_headers; i++)
    {
        switch (i)
        {
        case 0:
            headers[i] = "day";
            break;
        case 1:
            headers[i] = "hour";
            break;
        case 2:
            headers[i] = "minute";
            break;
        default:
            headers[i] = station_list[i - 3]->station_id;
            break;
        }
    }

    for (int i = 0; i < num_headers; i++)
    {
        if (i == (num_headers - 1))
            add_token(headers[i], true, fp);
        else
            add_token(headers[i], false, fp);
    }

    return fp;
}

// function to add a line to the output file
void write_line_to_output(Messstation **station_list, int line_num, FILE *fp)
{

    // write day/hour/minute
    int day = (line_num / 1440) + 1;
    int hour = (line_num / 60) % 24;
    int minute = line_num % 60;

    char string[10];

    int num_entries = num_stations + 3;

    for (int i = 0; i < num_entries; i++)
    {
        switch (i)
        {
        case 0:
            sprintf(string, "%d", day);
            add_token(string, false, fp);
            break;
        case 1:
            sprintf(string, "%d", hour);
            add_token(string, false, fp);
            break;
        case 2:
            sprintf(string, "%d", minute);
            add_token(string, false, fp);
            break;
        default:
            Messstation *station = station_list[i - 3];
            sprintf(string, "%.3f", station->temp_change_adjusted);
            if (i != (num_entries - 1))
                add_token(string, false, fp);
            else
                add_token(string, true, fp);
        }
    }
    return;
}

//**************MAIN FUNCTIONS*****************************************************

// WEATHER SIMULATION FUNCTION
void temperature_changes(Messstation **station_list, int time_steps)
{

    FILE *output = create_output(station_list);
    Messstation *station;

    int minute_count, i, j;
    int count_common = 0;
    // temperature influence form neighbours
    float neighbour_avg = 0.0f;
    int neighbour_count = 0;
#pragma omp parallel shared(output, station_list, time_steps, count_common) private(minute_count, i, j, station, neighbour_avg, neighbour_count)
    {
#pragma omp for schedule(static)
        for (minute_count = 0; minute_count < time_steps; minute_count++)
        {

            for (i = 0; i < num_stations; i++)
            {
                station = station_list[i];
                if (minute_count == 0)
                    station->local_temp_change = 0;

                station->local_temp_change = station->measurements[minute_count] - station->measurements[minute_count - 1];
            }

            for (j = 0; j < num_stations; j++)
            {
                station = station_list[j];

                if (station->North != NULL)
                {
                    neighbour_avg += station->North->local_temp_change;
                    neighbour_count++;
                }
                if (station->West != NULL)
                {
                    neighbour_avg += station->West->local_temp_change;
                    neighbour_count++;
                }
                if (station->South != NULL)
                {
                    neighbour_avg += station->South->local_temp_change;
                    neighbour_count++;
                }
                if (station->East != NULL)
                {
                    neighbour_avg += station->East->local_temp_change;
                    neighbour_count++;
                }
                if (neighbour_count > 0)
                {
                    neighbour_avg /= neighbour_count;
                    // ich habe die lokale temperaturänderung mehr gewicht gegeben, deswegen /2 (also die locale Tempreaturänderung hat dasselbe Gewicht wie der Mittelwert aller Nachbarn)
                    station->temp_change_adjusted = (station->local_temp_change + neighbour_avg) / 2;
                }
            }

// this will proably slow down the program: Instead it  probably would be faster to write to different output files and then merge them, but too much work xd
#pragma omp critical
            {
                count_common++;
                write_line_to_output(station_list, minute_count, output);
            }

            progress_bar(time_steps, count_common);
        }
    }
    fclose(output);
    return;
}

// Main FUnction
int main(void)
{

    // to time duration of the function
    double start_time = omp_get_wtime();

    char *dir_path = "MEVIS_DATEN";
    num_stations = count_files(dir_path);
    // both of the following functions need num_stations to work
    Messstation **station_list = init_data();
    find_neighbours(station_list);

    // WEATHER SIMULATION

    int minute_count;
    int minutes_in_year = 525600;

    temperature_changes(station_list, minutes_in_year);

    // free allocated memory
    for (int i = 0; i < num_stations; i++)
    {
        free(station_list[i]);
    }
    free(station_list);

    double end_time = omp_get_wtime();
    double time_taken = end_time-start_time;

    printf("The runtime of the programm was: %f seconds\n", time_taken);

    return 0;
}