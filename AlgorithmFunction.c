#include "algorithmFunction.h"
#include <stdio.h>


int deterministicAlgorithm(DataEntry entry, bool episodeState, int episodeCount){ 
    if(!episodeState){  //If the user is not in an episode
        bool tempValid = tempCheckNE(entry.temp);
        bool skinCondValid = skinCondCheckNE(entry.skinCond);
        bool heartRateValid = heartRateCheckNE(entry.heartRate);
        if(tempValid && skinCondValid && heartRateValid){
            printf("All parameters are within normal ranges.\n");
            episodeCount++;
        } else {
            printf("One or more parameters are out of range!\n");
            if(episodeCount > 0){
                episodeCount--;
            }
        }

        if(episodeCount >= 10){
            printf("User is in an episode!\n");
        } else {
            printf("User is not in an episode.\n");
        }

    } else {  //If the user is in an episode already
        bool tempValid = tempCheckE(entry.temp);
        bool skinCondValid = skinCondCheckE(entry.skinCond);
        bool heartRateValid = heartRateCheckE(entry.heartRate);
        if(tempValid && skinCondValid && heartRateValid){
            printf("All parameters suggest Episode is continuing.\n");
            if(episodeCount < 10){
                episodeCount++;
            }
        } else {
            printf("The episode might not continue.\n");
            if(episodeCount > 0){
                episodeCount--;
            }
        }

        if(episodeCount < 3){
            printf("User is not in an episode.\n");
        } else {
            printf("User is in an episode!\n");
        }
    }

    return episodeCount; // Return the updated episode count
}

//The three checks for the parameters when there's currently no episode.
bool tempCheckNE(int temp){
    if(temp <= 38){
        return true;
    }
    return false;
}
bool skinCondCheckNE(float skinCond){
    if(skinCond >= 0.7){
        return true;
    }
    return false;
}
bool heartRateCheckNE(float heartRate){
    if(heartRate >= 100){
        return true;
    }
    return false;
}

//The three checks for the parameters when there's currently an episode.
bool tempCheckE(int temp){
    if(temp >= 38){
        return true;
    }
    return false;
}
bool skinCondCheckE(float skinCond){
    if(skinCond < 0.7){
        return true;
    }
    return false;
}
bool heartRateCheckE(float heartRate){
    if(heartRate < 100){
        return true;
    }
    return false;
}

void main() {
    DataEntry entry;
    entry.temp = 45;
    entry.skinCond = 0.5;
    entry.heartRate = 80;
    bool episodeState = false;
    int episodeCount = 0;

    while(!episodeState) {
        episodeCount = deterministicAlgorithm(entry, episodeState, episodeCount);
        if(episodeCount >= 10) {
            episodeState = true; // User enters an episode
        }
        entry.temp -= 1;
        entry.skinCond += 0.05;
        entry.heartRate += 5;
    }
}