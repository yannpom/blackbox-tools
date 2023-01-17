#include <Python.h>

#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
#include <math.h>

#include <errno.h>
#include <fcntl.h>

#define _USE_MATH_DEFINES
#include <math.h>

#ifdef WIN32
    #include <io.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef WIN32
    #include "getopt.h"
#else
    #include <getopt.h>
#endif

#include "parser.h"
#include "platform.h"
#include "tools.h"
#include "gpxwriter.h"
#include "imu.h"
#include "battery.h"
#include "units.h"
#include "stats.h"

static int64_t lastFrameTime;
static uint32_t lastFrameIteration;

static int64_t bufferedSlowFrame[FLIGHT_LOG_MAX_FIELDS];

typedef struct field_t {
    PyObject* py;
    const char * name;
} field_t;


typedef struct decode_result_t {
    int field_count;
    field_t data[200];
} decode_result_t;

decode_result_t decode_result = {};


void onEvent(flightLog_t *log, flightLogEvent_t *event) {}

static void updateSimulations(flightLog_t *log, int64_t *frame, int64_t currentTime)
{
    int16_t gyroADC[3];
    int16_t accSmooth[3];
    int16_t magADC[3];

    bool hasMag = log->mainFieldIndexes.magADC[0] > -1;
    bool hasThrottle = log->mainFieldIndexes.rcCommand[3] != -1;
    bool hasAmperageADC = log->mainFieldIndexes.amperageLatest != -1;

    int i;
}

void outputMainFrameFields(flightLog_t *log, int64_t frameTime, int64_t *frame)
{
    //const float accScale = 9.81 * flightlogAccelerationRawToGs(log, 1);
    const float setpointScale = M_PI/180.0; // deg to rad


    for (int i=0; i<log->frameDefs['I'].fieldCount; i++) {
        float value = frame[i];
        if (strncmp("gyroADC", decode_result.data[i].name, 7) == 0) {
            value = flightlogGyroToRadiansPerSecond(log, value);
        } else if (strncmp("setpoint", decode_result.data[i].name, 8) == 0) {
            value *= setpointScale;
        } else if (strncmp("debug", decode_result.data[i].name, 5) == 0) {
            value = flightlogGyroToRadiansPerSecond(log, value);
        } else if (strncmp("axis", decode_result.data[i].name, 4) == 0) {
            value /= 2048.0;
        }
        PyList_Append(decode_result.data[i].py, PyFloat_FromDouble(value));
    }
}

void onFrameReady(flightLog_t *log, bool frameValid, int64_t *frame, uint8_t frameType, int fieldCount, int frameOffset, int frameSize)
{
    switch (frameType) {
        // case 'G':
        //     if (frameValid) {
        //         outputGPSFrame(log, frame);
        //     }
        // break;
        case 'S':
            //printf("FRAME S\n");
            if (frameValid) {
                memcpy(bufferedSlowFrame, frame, sizeof(bufferedSlowFrame));
            }
        break;
        case 'P':
        case 'I':
            if (frameValid) {
                updateSimulations(log, frame, lastFrameTime);
                lastFrameIteration = (uint32_t) frame[FLIGHT_LOG_FIELD_INDEX_ITERATION];
                lastFrameTime = frame[FLIGHT_LOG_FIELD_INDEX_TIME];
                outputMainFrameFields(log, frameValid ? frame[FLIGHT_LOG_FIELD_INDEX_TIME] : -1, frame);
            }
        break;
    }
}

void onMetadataReady(flightLog_t *log)
{
    //printf("onMetadataReady I %d S %d\n", log->frameDefs['I'].fieldCount, log->frameDefs['S'].fieldCount);

    // for (int i = 0; i < log->frameDefs['S'].fieldCount; i++) {
    //     printf("S: %s\n", log->frameDefs['S'].fieldName[i]);
    // }

    // for (int i=0; i<3; ++i) {
    //     printf("%d %d %d %d\n", log->pidValues[i].p, log->pidValues[i].i, log->pidValues[i].d, log->pidValues[i].ff);
    // }


    decode_result.field_count = log->frameDefs['I'].fieldCount;
    for (int i=0; i<decode_result.field_count; ++i) {
        decode_result.data[i].py = PyList_New(0);
        decode_result.data[i].name = log->frameDefs['I'].fieldName[i];
    }
    
    if (log->frameDefs['I'].fieldCount == 0) {
        fprintf(stderr, "No fields found in log, is it missing its header?\n");
        return;
    }
}

void resetParseState() {
    memset(bufferedSlowFrame, 0, sizeof(bufferedSlowFrame));
    lastFrameIteration = (uint32_t) -1;
    lastFrameTime = -1;
}

double parseDegreesMinutes(const char *s)
{
    int combined = (int) round(atof(s) * 100);

    int degrees = combined / 100;
    int minutes = combined % 100;

    return degrees + (double) minutes / 60;
}

static PyObject *py_decode(PyObject *self, PyObject *args) {
    char *filename;

    if (!PyArg_ParseTuple(args, "s", &filename)) {
        return NULL;
    }

    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        PyErr_SetString(PyExc_ValueError, "Failed to open file");
        return NULL;
    }

    flightLog_t *log = flightLogCreate(fd);

    if (!log) {
        PyErr_SetString(PyExc_ValueError, "Failed to read log");
        return NULL;
    }

    if (log->logCount == 0) {
        PyErr_SetString(PyExc_ValueError, "Could not find header in log file");
        return NULL;
    }

    PyObject* flights = PyList_New(0);

    for (int logIndex = 0; logIndex < log->logCount; logIndex++) {    
        resetParseState();

        if ((log->private->stream->mapping.stats.st_mode & S_IFMT) == S_IFCHR) { //prime data buffer with data
            fillSerialBuffer(log->private->stream, FLIGHT_LOG_MAX_FRAME_SERIAL_BUFFER_LENGTH, NULL);
        }

        int success = flightLogParse(log, logIndex, onMetadataReady, onFrameReady, onEvent, false);

        PyObject* dict_data = PyDict_New();

        for (int i=0; i<decode_result.field_count; ++i) {
            PyDict_SetItemString(dict_data, decode_result.data[i].name, decode_result.data[i].py);
        }

        PyObject* list_pid = PyList_New(3);
        for (int i=0; i<3; ++i) {
            PyObject* dict_pid = PyDict_New();
            PyList_SetItem(list_pid, i, dict_pid);
            PyDict_SetItemString(dict_pid, "p", PyLong_FromLong(log->pidValues[i].p));
            PyDict_SetItemString(dict_pid, "i", PyLong_FromLong(log->pidValues[i].i));
            PyDict_SetItemString(dict_pid, "d", PyLong_FromLong(log->pidValues[i].d));
            PyDict_SetItemString(dict_pid, "ff", PyLong_FromLong(log->pidValues[i].ff));
        }

        PyObject* dict = PyDict_New();
        PyDict_SetItemString(dict, "data", dict_data);
        PyDict_SetItemString(dict, "pid", list_pid);

        PyList_Append(flights, dict);
    }

    flightLogDestroy(log);    

    return flights;
}

static PyMethodDef methods[] = {
    {"decode", py_decode, METH_VARARGS, "Function for decoding BlackBox file"},
    {NULL, NULL, 0, NULL}
};


static struct PyModuleDef module = {
    PyModuleDef_HEAD_INIT,
    "_blackbox_tools",
    "C library for decoding Betaflight Blackbox files",
    -1,
    methods
};


PyMODINIT_FUNC PyInit__blackbox_tools(void) {
    return PyModule_Create(&module);
};
