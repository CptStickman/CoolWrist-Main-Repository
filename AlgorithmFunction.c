#include "algorithmFunction.h"
#include <stdio.h>

int deterministicAlgorithm(DataEntry entry, bool episodeState, int episodeCount, DataEntry normalStats){ 
    if(!episodeState){  //If the user is not in an episode
        bool tempValid = tempCheckNE(entry.temp, normalStats.temp);
        bool skinCondValid = skinCondCheckNE(entry.skinCond, normalStats.skinCond);
        bool heartRateValid = heartRateCheckNE(entry.heartRate, normalStats.heartRate);
        if(tempValid && skinCondValid && heartRateValid){
            printf("All parameters are leaning towards an episode.\n");
            episodeCount++;
        } else {
            printf("Some aren't suggesting an episode\n");
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
        bool tempValid = tempCheckE(entry.temp, normalStats.temp);
        bool skinCondValid = skinCondCheckE(entry.skinCond, normalStats.skinCond);
        bool heartRateValid = heartRateCheckE(entry.heartRate, normalStats.heartRate);
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

//The three checks to determine if an episode is starting.
bool tempCheckNE(float temp, float normalTemp){
    if(temp <= normalTemp - normalTemp * 0.2){
        return true;
    }
    return false;
}
bool skinCondCheckNE(float skinCond, float normalSkinCond){
    if(skinCond >= normalSkinCond + normalSkinCond * 0.2){
        return true;
    }
    return false;
}
bool heartRateCheckNE(float heartRate, float normalHeartRate){
    if(heartRate >= normalHeartRate + normalHeartRate * 0.2){
        return true;
    }
    return false;
}

//The three checks to determine if an episode is ending.
bool tempCheckE(float temp, float normalTemp){
    if(temp >= normalTemp - normalTemp * 0.2){
        return true;
    }
    return false;
}
bool skinCondCheckE(float skinCond, float normalSkinCond){
    if(skinCond < normalSkinCond + normalSkinCond * 0.2){
        return true;
    }
    return false;
}
bool heartRateCheckE(float heartRate, float normalHeartRate){
    if(heartRate < normalHeartRate - normalHeartRate * 0.2){
        return true;
    }
    return false;
}

void main() {
    DataEntry normalStats = {37, 0.7, 75}; // Example normal stats

    DataEntry entry;
    entry.temp = 45;
    entry.skinCond = 0.5;
    entry.heartRate = 80;
    bool episodeState = false;
    int episodeCount = 0;

    while(!episodeState) {
        episodeCount = deterministicAlgorithm(entry, episodeState, episodeCount, normalStats);
        if(episodeCount >= 10) {
            episodeState = true; // User enters an episode
        }
        entry.temp -= 1;
        entry.skinCond += 0.05;
        entry.heartRate += 5;
    }
}