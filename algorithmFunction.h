#include <stdbool.h>

typedef struct DataEntry {
    float temp;
    float skinCond;
    float heartRate;
} DataEntry;

//The data entry and whether the user is in an episode or not.
int deterministicAlgorithm(DataEntry entry, bool episodeStat, int episodeCount, DataEntry normalStats);  

bool tempCheckNE(float temp, float normalTemp);
bool skinCondCheckNE(float skinCond, float normalSkinCond);
bool heartRateCheckNE(float heartRate, float normalHeartRate);

bool tempCheckE(float temp, float normalTemp);
bool skinCondCheckE(float skinCond, float normalSkinCond);
bool heartRateCheckE(float heartRate, float normalHeartRate);