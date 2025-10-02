#include <stdbool.h>

typedef struct DataEntry {
    int temp;
    float skinCond;
    float heartRate;
} DataEntry;

//The data entry and whether the user is in an episode or not.
int deterministicAlgorithm(DataEntry entry, bool episodeStat, int episodeCount);  

bool tempCheckNE(int temp);
bool skinCondCheckNE(float skinCond);
bool heartRateCheckNE(float heartRate);

bool tempCheckE(int temp);
bool skinCondCheckE(float skinCond);
bool heartRateCheckE(float heartRate);