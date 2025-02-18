#ifndef PARSER_H_
#define PARSER_H_

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "platform.h"
#include "stream.h"

#include "blackbox_fielddefs.h"

#define FLIGHT_LOG_MAX_LOGS_IN_FILE 1000
#define FLIGHT_LOG_MAX_FIELDS 128

#define FLIGHT_LOG_FIELD_INDEX_ITERATION 0
#define FLIGHT_LOG_FIELD_INDEX_TIME 1

#define FLIGHT_LOG_MAX_MOTORS 8
#define FLIGHT_LOG_MAX_SERVOS 8

typedef enum FirmwareType {
    FIRMWARE_TYPE_UNKNOWN = 0,
    FIRMWARE_TYPE_BASEFLIGHT,
    FIRMWARE_TYPE_CLEANFLIGHT,
	FIRMWARE_TYPE_BETAFLIGHT
} FirmwareType;

typedef struct flightLogFrameStatistics_t {
    uint32_t bytes;
    // Frames decoded to the right length and had reasonable data in them:
    uint32_t validCount;

    // Frames decoded to the right length but the data looked bad so they were rejected, or stream was desynced from previous lost frames:
    uint32_t desyncCount;

    // Frames didn't decode to the right length at all
    uint32_t corruptCount;

    uint32_t sizeCount[FLIGHT_LOG_MAX_FRAME_LENGTH + 1];
} flightLogFrameStatistics_t;

typedef struct flightLogFieldStatistics_t {
    int64_t min, max;
} flightLogFieldStatistics_t;

typedef struct flightLogStatistics_t {
    uint32_t totalBytes;

    // Number of frames that failed to decode:
    uint32_t totalCorruptFrames;

    //If our sampling rate is less than 1, we won't log every loop iteration, and that is accounted for here:
    uint32_t intentionallyAbsentIterations;

    bool haveFieldStats;
    flightLogFieldStatistics_t field[FLIGHT_LOG_MAX_FIELDS];
    flightLogFrameStatistics_t frame[256];
} flightLogStatistics_t;



/*
 * We provide a list of indexes of well-known fields to save callers the trouble of comparing field name strings
 * to hunt down the fields they're interested in. Absent fields will have index -1.
 */
typedef struct gpsGFieldIndexes_t {
    int time;
    int GPS_numSat;
    int GPS_coord[2];
    int GPS_altitude;
    int GPS_speed;
    int GPS_ground_course;
} gpsGFieldIndexes_t;

typedef struct gpsHFieldIndexes_t {
    int GPS_home[2];
} gpsHFieldIndexes_t;

typedef struct slowFieldIndexes_t {
    int flightModeFlags;
    int stateFlags;
    int failsafePhase;
} slowFieldIndexes_t;

typedef struct mainFieldIndexes_t {
    int loopIteration;
    int time;

    int pid[3][3]; //First dimension is [P, I, D], second dimension is axis

    int rcCommand[4];

    int vbatLatest, amperageLatest;
    int magADC[3];
    int BaroAlt;
    int sonarRaw;
    int rssi;

    int gyroADC[3];
    int accSmooth[3];

    int motor[FLIGHT_LOG_MAX_MOTORS];
    int servo[FLIGHT_LOG_MAX_SERVOS];
} mainFieldIndexes_t;

/**
 * Information about the system configuration of the craft being logged (aids in interpretation
 * of the log data).
 */
typedef struct flightLogSysConfig_t {
    int minthrottle, maxthrottle;
    int motorOutputLow, motorOutputHigh; // Betaflight
    unsigned int rcRate, yawRate;

    // Calibration constants from the hardware sensors:
    uint16_t acc_1G;
    float gyroScale;

    uint8_t vbatscale;
    uint8_t vbatmaxcellvoltage;
    uint8_t vbatmincellvoltage;
    uint8_t vbatwarningcellvoltage;

    int16_t currentMeterOffset, currentMeterScale;

    uint16_t vbatref;

    FirmwareType firmwareType;
} flightLogSysConfig_t;

typedef struct flightLogFrameDef_t {
    char *namesLine; // The parser owns this memory to store the field names for this frame type (as a single string)

    int fieldCount;

    char *fieldName[FLIGHT_LOG_MAX_FIELDS];
    
    int fieldSigned[FLIGHT_LOG_MAX_FIELDS];
    int fieldWidth[FLIGHT_LOG_MAX_FIELDS];
    int predictor[FLIGHT_LOG_MAX_FIELDS];
    int encoding[FLIGHT_LOG_MAX_FIELDS];
} flightLogFrameDef_t;

struct flightLogPrivate_t;

typedef struct pid_values_t {
    int p;
    int i;
    int d;
    int ff;
} pid_values_t;

typedef struct flightLog_t {
	time_t dateTime; //GPS start date and time
    flightLogStatistics_t stats;

    //Information about fields which we need to decode them properly
    flightLogFrameDef_t frameDefs[256];

    flightLogSysConfig_t sysConfig;

    //Information about log sections:
    const char *logBegin[FLIGHT_LOG_MAX_LOGS_IN_FILE + 1];
    int logCount;

    unsigned int frameIntervalI;
    unsigned int frameIntervalPNum, frameIntervalPDenom;

    mainFieldIndexes_t mainFieldIndexes;
    gpsGFieldIndexes_t gpsFieldIndexes;
    gpsHFieldIndexes_t gpsHomeFieldIndexes;
    slowFieldIndexes_t slowFieldIndexes;

    pid_values_t pidValues[3];

    struct flightLogPrivate_t *private;
} flightLog_t;



typedef void (*FlightLogMetadataReady)(flightLog_t *log);
typedef void (*FlightLogFrameReady)(flightLog_t *log, bool frameValid, int64_t *frame, uint8_t frameType, int fieldCount, int frameOffset, int frameSize);
typedef void (*FlightLogEventReady)(flightLog_t *log, flightLogEvent_t *event);

typedef struct flightLogPrivate_t
{
    int dataVersion;

    char fcVersion[30];

    // Blackbox state:
    int64_t blackboxHistoryRing[3][FLIGHT_LOG_MAX_FIELDS];

    /* Points into blackboxHistoryRing to give us a circular buffer.
     *
     * 0 - space to decode new frames into, 1 - previous frame, 2 - previous previous frame
     *
     * Previous frame pointers are NULL when no valid history exists of that age.
     */
    int64_t* mainHistory[3];
    bool mainStreamIsValid;
    // When 32-bit time values roll over to zero, we add 2^32 to this accumulator so it can be added to the time:
    int64_t timeRolloverAccumulator;

    int64_t gpsHomeHistory[2][FLIGHT_LOG_MAX_FIELDS]; // 0 - space to decode new frames into, 1 - previous frame
    bool gpsHomeIsValid;

    //Because these events don't depend on previous events, we don't keep copies of the old state, just the current one:
    flightLogEvent_t lastEvent;
    int64_t lastGPS[FLIGHT_LOG_MAX_FIELDS];
    int64_t lastSlow[FLIGHT_LOG_MAX_FIELDS];

    // How many intentionally un-logged frames did we skip over before we decoded the current frame?
    uint32_t lastSkippedFrames;
    
    // Details about the last main frame that was successfully parsed
    uint32_t lastMainFrameIteration;
    int64_t lastMainFrameTime;

    // Event handlers:
    FlightLogMetadataReady onMetadataReady;
    FlightLogFrameReady onFrameReady;
    FlightLogEventReady onEvent;

    mmapStream_t *stream;
} flightLogPrivate_t;

flightLog_t* flightLogCreate(int fd);

int flightLogEstimateNumCells(flightLog_t *log);

unsigned int flightLogVbatADCToMillivolts(flightLog_t *log, uint16_t vbatADC);
int flightLogAmperageADCToMilliamps(flightLog_t *log, uint16_t amperageADC);
double flightlogGyroToRadiansPerSecond(flightLog_t *log, int32_t gyroRaw);
double flightlogAccelerationRawToGs(flightLog_t *log, int32_t accRaw);
void flightlogFlightModeToString(uint32_t flightMode, char *dest, int destLen);
void flightlogFlightStateToString(uint32_t flightState, char *dest, int destLen);
void flightlogFailsafePhaseToString(uint8_t failsafePhase, char *dest, int destLen);

bool flightLogParse(flightLog_t *log, int logIndex, FlightLogMetadataReady onMetadataReady, FlightLogFrameReady onFrameReady, FlightLogEventReady onEvent, bool raw);
void flightLogDestroy(flightLog_t *log);

#endif

