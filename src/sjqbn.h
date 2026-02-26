/**
 * @file sjqbn.h
 * @brief Main header file for SJQBN library.
 * @version 1.0
 *
 * Delivers the SJQBN model 
 *
 */
#ifndef SJQBN_H
#define SJQBN_H

// Includes
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

#include "sjqbn_util.h"

/** Defines a return value of success */
#define SUCCESS 0
/** Defines a return value of failure */
#define FAIL 1

/* config string */
#define SJQBN_CONFIG_MAX 1000
#define SJQBN_DATASET_MAX 10

extern int sjqbn_ucvm_debug;
extern FILE *stderrfp;

// Structures
/** Defines a point (latitude, longitude, and depth) in WGS84 format */
typedef struct sjqbn_point_t {
	/** Longitude member of the point */
	double longitude;
	/** Latitude member of the point */
	double latitude;
	/** Depth member of the point */
	double depth;
} sjqbn_point_t;

/** Defines the material properties this model will retrieve. */
typedef struct sjqbn_properties_t {
	/** P-wave velocity in meters per second */
	double vp;
	/** S-wave velocity in meters per second */
	double vs;
	/** Density in g/m^3 */
	double rho;
	/** Qp */
	double qp;
	/** Qs */
	double qs;
} sjqbn_properties_t;

/**
Dimensions: 3
  dim[0] name=depth len=84
  dim[1] name=latitude len=381
  dim[2] name=longitude len=471
Total elements: 15073884
**/

/** The SJQBN configuration structure. */
typedef struct sjqbn_configuration_t {
	/** The zone of UTM projection */
	int utm_zone;
	/** The model directory */
	char model_dir[128];
        /** GTL on or off (1 or 0) */
        int gtl;
	/** interpolation (1 or 0) */
	int interpolation;

        /* how many datasets are in the model */
        int dataset_cnt;
        char *dataset_files[SJQBN_DATASET_MAX];  //strdup
	char *dataset_labels[SJQBN_DATASET_MAX]; // strdup
} sjqbn_configuration_t;

typedef struct sjqbn_model_t {
        int dataset_cnt;
        sjqbn_dataset_t *datasets[SJQBN_DATASET_MAX];
} sjqbn_model_t;


// Constants
/** The version of the model. */
extern const char *sjqbn_version_string;

// Variables
/** Set to 1 when the model is ready for query. */
extern int sjqbn_is_initialized;

/** Configuration parameters. */
extern sjqbn_configuration_t *sjqbn_configuration;

/** Holds pointers to the velocity model data OR indicates it can be read from file. */
extern sjqbn_model_t *sjqbn_velocity_model;

// UCVM API Required Functions

#ifdef DYNAMIC_LIBRARY

/** Initializes the model */
int model_init(const char *dir, const char *label);
/** Cleans up the model (frees memory, etc.) */
int model_finalize();
/** Returns version information */
int model_version(char *ver, int len);
/** Queries the model */
int model_query(sjqbn_point_t *points, sjqbn_properties_t *data, int numpts);

int (*get_model_init())(const char *, const char *);
int (*get_model_query())(sjqbn_point_t *, sjqbn_properties_t *, int);
int (*get_model_finalize())();
int (*get_model_version())(char *, int);

#endif

// SJQBN Related Functions

/** Initializes the model */
int sjqbn_init(const char *dir, const char *label);
/** Cleans up the model (frees memory, etc.) */
int sjqbn_finalize();
/** Returns version information */
int sjqbn_version(char *ver, int len);
/** Queries the model */
int sjqbn_query(sjqbn_point_t *points, sjqbn_properties_t *data, int numpts);

// Non-UCVM Helper Functions
//
/** Reads the configuration file and helper functions. */
int sjqbn_read_configuration(char *file, sjqbn_configuration_t *config);
int sjqbn_configuration_finalize(sjqbn_configuration_t *config);

/** Prints out the error string. */
void sjqbn_print_error(char *err);
/** Retrieves the value at a specified grid point in the model. */
void sjqbn_read_properties(int x, int y, int z, sjqbn_properties_t *data);
/** Attempts to malloc the model size in memory and read it in. */
int sjqbn_read_model(sjqbn_configuration_t *config, sjqbn_model_t *model, char* dir);
/** toggle debug flag **/
void sjqbn_setdebug();

/** helper function for velocity_model **/
int sjqbn_velocity_model_init(sjqbn_model_t *model);
int sjqbn_velocity_model_finalize(sjqbn_model_t *model);

/** parse JSON metadata blob per dataset **/
int _setup_a_dataset(sjqbn_configuration_t *conf, char *blobstr);

void _trimLast(char *str, char m);
void _splitline(char* lptr, char key[], char value[]);

#endif
