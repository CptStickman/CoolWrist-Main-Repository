#include "algorithmFunction.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int deterministicAlgorithm(DataEntry entry, bool episodeState, int episodeCount, DataEntry normalStats){ 
    if(!episodeState){  //If the user is not in an episode
        bool tempValid = tempCheckNE(entry.temp, normalStats.temp);
        bool skinCondValid = skinCondCheckNE(entry.skinCond, normalStats.skinCond);
        bool heartRateValid = heartRateCheckNE(entry.heartRate, normalStats.heartRate);
        if(tempValid && skinCondValid && heartRateValid){
            //printf("All parameters are leaning towards an episode.\n");
            episodeCount++;
        } else {
            //printf("Some aren't suggesting an episode\n");
            if(episodeCount > 0){
                episodeCount--;
            }
        }

        if(episodeCount >= 10){
            //printf("User is in an episode!\n");
        } else {
            //printf("User is not in an episode.\n");
        }

    } else {  //If the user is in an episode already
        bool tempValid = tempCheckE(entry.temp, normalStats.temp);
        bool skinCondValid = skinCondCheckE(entry.skinCond, normalStats.skinCond);
        bool heartRateValid = heartRateCheckE(entry.heartRate, normalStats.heartRate);
        if(tempValid && skinCondValid && heartRateValid){
            //printf("All parameters suggest Episode is continuing.\n");
            if(episodeCount > 0){
                episodeCount--;
            }
        } else {
            //printf("The episode might not continue.\n");
            if(episodeCount < 10){
                episodeCount++;
            }
        }

        if(episodeCount < 3){
            //printf("User is not in an episode.\n");
        } else {
            //printf("User is in an episode!\n");
        }
    }

    return episodeCount; // Return the updated episode count
}

//The three checks to determine if an episode is starting.
bool tempCheckNE(float temp, float normalTemp){
    if(temp <= normalTemp * 0.8){
        return true;
    }
    return false;
}
bool skinCondCheckNE(float skinCond, float normalSkinCond){
    if(skinCond <= normalSkinCond * 0.3){
        return true;
    }
    return false;
}
bool heartRateCheckNE(float heartRate, float normalHeartRate){
    if(heartRate >= normalHeartRate * 1.1){
        return true;
    }
    return false;
}

//The three checks to determine if an episode is ending.
bool tempCheckE(float temp, float normalTemp){
    if(temp > normalTemp * 0.8){
        return true;
    }
    return false;
}
bool skinCondCheckE(float skinCond, float normalSkinCond){
    if(skinCond > normalSkinCond * 0.3){
        return true;
    }
    return false;
}
bool heartRateCheckE(float heartRate, float normalHeartRate){
    if(heartRate < normalHeartRate * 1.1){
        return true;
    }
    return false;
}

void main() {
    FILE *inputFile = fopen("health_data.txt", "r");
    FILE *outputFile = fopen("health_data_algorithm.txt", "w");
    
    if (inputFile == NULL) {
        printf("Error: Could not open health_data.txt\n");
        return;
    }
    
    if (outputFile == NULL) {
        printf("Error: Could not create health_data_algorithm.txt\n");
        fclose(inputFile);
        return;
    }
    
    // Use pre-inputted baseline values
    DataEntry normalStats = {29, 1100, 75}; // Temp, SkinCond, HeartRate
    printf("Using pre-set baseline: Temp=%.2f, HR=%.2f, SC=%.2f\n", 
           normalStats.temp, normalStats.heartRate, normalStats.skinCond);
    
    // Process all data through algorithm
    bool episodeState = false;
    int episodeCount = 0;
    int lineNumber = 0;
    int totalEpisodesDetected = 0;
    char line[256];
    char time[16];
    
    fprintf(outputFile, "Time Temperature HeartRate SkinConductivity EpisodeCount AlgorithmPrediction ActualEpisode ActualStatus\n");
    
    printf("Processing health data through algorithm...\n");
    while (fgets(line, sizeof(line), inputFile)) {
        DataEntry entry;
        char actualEpisodeStatus[16];
        if (sscanf(line, "%s %f %f %f %s", time, &entry.temp, &entry.heartRate, &entry.skinCond, actualEpisodeStatus) == 5) {
            lineNumber++;
            
            // Determine actual episode status from data
            bool actualEpisode = (strncmp(actualEpisodeStatus, "E", 1) == 0);
            
            // Run algorithm
            episodeCount = deterministicAlgorithm(entry, episodeState, episodeCount, normalStats);
            
            // Update episode state based on episode count
            if (!episodeState && episodeCount >= 10) {
                episodeState = true;
                totalEpisodesDetected++;
                printf("Episode started at line %d (time %s)\n", lineNumber, time);
            } else if (episodeState && episodeCount < 3) {
                episodeState = false;
                printf("Episode ended at line %d (time %s)\n", lineNumber, time);
            }
            
            // Write results to output file
            fprintf(outputFile, "%s %.2f %.2f %.2f %d %s %s %s\n", 
                   time, entry.temp, entry.heartRate, entry.skinCond, 
                   episodeCount, episodeState ? "YES" : "NO", 
                   actualEpisode ? "YES" : "NO", actualEpisodeStatus);
        }
    }
    
    fclose(inputFile);
    fclose(outputFile);
    
    printf("Algorithm processing complete!\n");
    printf("Results written to health_data_algorithm.txt\n");
    printf("Total lines processed: %d\n", lineNumber);
    printf("Total episodes detected by algorithm: %d\n", totalEpisodesDetected);
}