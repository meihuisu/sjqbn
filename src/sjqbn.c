/**
 * @file sjqbn.c
 * @brief Main file for sjqbn model
 * @version 1.0
 *
 * @section DESCRIPTION
 *
 */

#include <limits.h>
#include "ucvm_model_dtypes.h"
#include "sjqbn.h"
#include "sjqbn_util.h"
#include "um_netcdf.h"
#include "cJSON.h"

int sjqbn_ucvm_debug=0;
FILE *stderrfp=NULL;

int TooBig=1;
int _ON=0;

/** The config of the model */
char *sjqbn_config_string=NULL;
int sjqbn_config_sz=0;

// Constants
/** The version of the model. */
const char *sjqbn_version_string = "sjqbn";

// Variables
/** Set to 1 when the model is ready for query. */
int sjqbn_is_initialized = 0;

char sjqbn_data_directory[128];

/** Configuration parameters. */
sjqbn_configuration_t *sjqbn_configuration;
/** Holds pointers to the velocity model data OR indicates it can be read from file. */
sjqbn_model_t *sjqbn_velocity_model;

/**
 * Initializes the sjqbn plugin model within the UCVM framework. In order to initialize
 * the model, we must provide the UCVM install path and optionally a place in memory
 * where the model already exists.
 *
 * @param dir The directory in which UCVM has been installed.
 * @param label A unique identifier for the velocity model.
 * @return Success or failure, if initialization was successful.
 */
int sjqbn_init(const char *dir, const char *label) {
    int tempVal = 0;
    char configbuf[512];
    double north_height_m = 0, east_width_m = 0, rotation_angle = 0;

    if(sjqbn_ucvm_debug) {
      stderrfp = fopen("sjqbn_debug.log", "w+");
      fprintf(stderrfp," ===== START ===== \n");
    }

    // Initialize variables.
    
    sjqbn_configuration = calloc(1, sizeof(sjqbn_configuration_t));
    sjqbn_config_string = calloc(SJQBN_CONFIG_MAX, sizeof(char));
    sjqbn_config_string[0]='\0';
    sjqbn_config_sz=0;

    sjqbn_velocity_model = calloc(1, sizeof(sjqbn_model_t));
    sjqbn_velocity_model_init(sjqbn_velocity_model);

    // Configuration file location.
    sprintf(configbuf, "%s/model/%s/data/config", dir, label);

    // Read the configuration file.
    if (sjqbn_read_configuration(configbuf, sjqbn_configuration) != UCVM_MODEL_CODE_SUCCESS) {
           // Try another, when is running in standalone mode..
       sprintf(configbuf, "%s/data/config", dir);
       if (sjqbn_read_configuration(configbuf, sjqbn_configuration) != UCVM_MODEL_CODE_SUCCESS) {
           sjqbn_print_error("No configuration file was found to read from.");
           return UCVM_MODEL_CODE_ERROR;
           } else {
           // Set up the data directory.
               sprintf(sjqbn_data_directory, "%s/data/%s", dir, sjqbn_configuration->model_dir);
       }
       } else {
           // Set up the data directory.
           sprintf(sjqbn_data_directory, "%s/model/%s/data/%s", dir, label, sjqbn_configuration->model_dir);
    }

    // Can we allocate the model, or parts of it, to memory. If so, we do.
    sjqbn_velocity_model->dataset_cnt=sjqbn_configuration->dataset_cnt;
    tempVal = sjqbn_read_model(sjqbn_configuration, sjqbn_velocity_model, sjqbn_data_directory);

    if (tempVal == SUCCESS) {
      if(sjqbn_ucvm_debug) {
        fprintf(stderrfp, "WARNING: Could not load model into memory. Reading the model from the\n");
        fprintf(stderrfp, "hard disk may result in slow performance.\n");
      }
    } else if (tempVal == FAIL) {
        sjqbn_print_error("No model file was found to read from.");
        return FAIL;
    }

    // setup config_string 
    sprintf(sjqbn_config_string,"config = %s\n",configbuf);
    sjqbn_config_sz=1;

    // Let everyone know that we are initialized and ready for business.
    sjqbn_is_initialized = 1;

    return SUCCESS;
}

/**
 * Queries sjqbn at the given points and returns the data that it finds.
 *
 * @param points The points at which the queries will be made.
 * @param data The data that will be returned (Vp, Vs, rho, Qs, and/or Qp).
 * @param numpoints The total number of points to query.
 * @return SUCCESS or FAIL.
 */
int sjqbn_query(sjqbn_point_t *points, sjqbn_properties_t *data, int numpoints) {

if(sjqbn_ucvm_debug){ fprintf(stderrfp,"\ncalling sjqbn_query with %d numpoints\n",numpoints); }

    int data_idx = 0;
    nc_type vtype=NC_FLOAT;

    /* iterate through the dataset to see where does the point fall into */
    /* for now assume there is only 1 dataset */
    sjqbn_dataset_t *dataset= sjqbn_velocity_model->datasets[data_idx];

    float *lon_list=dataset->longitudes;
    float *lat_list=dataset->latitudes;
    float *dep_list=dataset->depths;
    int nx=dataset->nx;
    int ny=dataset->ny;
    int nz=dataset->nz;

    // hold current working buffers
    float *tmp_vp_buffer=NULL;
    float *tmp_vs_buffer=NULL;
    float *tmp_rho_buffer=NULL;

    float lon_f;
    float lat_f;
    float dep_f;
    
    int lon_idx, first_lon_idx;
    int lat_idx, first_lat_idx;
    int dep_idx, first_dep_idx;

    int same_lon_idx=1;
    int same_lat_idx=1;
    int same_dep_idx=1;

    size_t *dep_idx_buffer;
    size_t *lat_idx_buffer;
    size_t *lon_idx_buffer;

    int offset;

// hold the result
    dep_idx_buffer = malloc(numpoints * sizeof(size_t));
    if (!dep_idx_buffer) { fprintf(stderr, "malloc failed\n");}
    lat_idx_buffer = malloc(numpoints * sizeof(size_t));
    if (!lat_idx_buffer) { fprintf(stderr, "malloc failed\n");}
    lon_idx_buffer = malloc(numpoints * sizeof(size_t));
    if (!lon_idx_buffer) { fprintf(stderr, "malloc failed\n");}

// iterate through all the points and compose the buffers for all index
    for(int i=0; i<numpoints; i++) {
        data[i].vp = -1;
        data[i].vs = -1;
        data[i].rho = -1;
        data[i].qp = -1;
        data[i].qs = -1;

        lon_f=points[i].longitude;
        lat_f=points[i].latitude;
        dep_f=points[i].depth;

if(sjqbn_ucvm_debug){ if(i<5) fprintf(stderrfp,"\nfirst %d, float lon/lat/dep = %f/%f/%f\n", i, lon_f, lat_f, dep_f); }

        lon_idx=find_buffer_idx((float *)lon_list,nx,lon_f);
        lat_idx=find_buffer_idx((float *)lat_list,ny,lat_f);
        dep_idx=find_buffer_idx((float *)dep_list,nz,dep_f);

if(sjqbn_ucvm_debug){ if(i<5) fprintf(stderrfp,"    with idx lon/lat/dep = %d/%d/%d\n", lon_idx, lat_idx, dep_idx); }

        if(i==0) {
            first_dep_idx=dep_idx;
            first_lon_idx=lon_idx;
            first_lat_idx=lat_idx;
        }
        dep_idx_buffer[i]=dep_idx;
        lon_idx_buffer[i]=lon_idx;
        lat_idx_buffer[i]=lat_idx;

        if(dep_idx != first_dep_idx) same_dep_idx=0;
        if(lon_idx != first_lon_idx) same_lon_idx=0;
        if(lat_idx != first_lat_idx) same_lat_idx=0;
    }

    int bucket_cnt=bucket_an_array(dep_idx_buffer, numpoints);

// handle access 
// if not TooBig, grab from in-memory buffer one at a time
// if tooBig, then collect up all the index list and make just one call and
// retrieve and disperse the result back into data

    if(!TooBig) { 
if(sjqbn_ucvm_debug){ fprintf(stderrfp,">> In-Memory access \n"); }

// it is not too big, extract from data buffers one at a time
        tmp_vp_buffer=dataset->vp_buffer;
        tmp_vs_buffer=dataset->vs_buffer;
        tmp_rho_buffer=dataset->rho_buffer;

        for(int i=0; i<numpoints; i++) {
            dep_idx=dep_idx_buffer[i];
            lat_idx=lat_idx_buffer[i];
            lon_idx=lon_idx_buffer[i];

// offset= (dep_idx)*(lat_cnt * lon_cnt)+(lat_idx)*(lon_cnt)+lon_idx
            offset= (dep_idx)*(ny * nx)+(lat_idx)*(nx)+lon_idx;

if(sjqbn_ucvm_debug) { fprintf(stderrfp,"\nTarget offset %d : idx lon/lat/dep = %d/%d/%d\n", offset,lon_idx, lat_idx, dep_idx); }

            data[i].vp = tmp_vp_buffer[offset];
            data[i].vs = tmp_vs_buffer[offset];
            data[i].rho =tmp_rho_buffer[offset];
        }
    } else { 

// special case, if numpoints < 5 just handle as random 

// it is too big, extract data from external data file 
// group netcdf access to 
//   column based(same lat idx an same lon idx),
//   or
//   layer based(same depth idx) 
//   or random method

// if same_lon_idx and same_lat_idx,
// it is depth profile
//    extract the whole column with dataset->nz for different set 
//    and then extract data from buffer one at a time using dep_idx 
        if(numpoints > 5  && same_lon_idx && same_lat_idx ) {
		 
          // grab a col from cache
            sjqbn_cache_col_t *col=find_a_cache_col(dataset, first_lat_idx, first_lon_idx); 
if(sjqbn_ucvm_debug){ fprintf(stderrfp,">> Using Col cache - %d\n", dataset->col_cache_cnt); }
          
            tmp_vp_buffer=col->col_vp_buffer;
            tmp_vs_buffer=col->col_vs_buffer;
            tmp_rho_buffer=col->col_rho_buffer;

            for(int i=0; i<numpoints; i++) { 
                offset=dep_idx_buffer[i];

                data[i].vp = tmp_vp_buffer[offset];
                data[i].vs = tmp_vs_buffer[offset];
                data[i].rho =tmp_rho_buffer[offset];
            }

// if just same_dep_idx, it is a horizontal slice,
// extract the whole layer using dataset->nx and dataset->ny
// and then extract data from buffer using lon_idx, and lat_idx
        } else if (numpoints > 5 && same_dep_idx) { 
          // grab a col from cache
            sjqbn_cache_layer_t *layer=find_a_cache_layer(dataset, first_dep_idx);
if(sjqbn_ucvm_debug){ fprintf(stderrfp,">> Using Layer cache - %d\n", dataset->layer_cache_cnt); }
          
            tmp_vp_buffer=layer->layer_vp_buffer;
            tmp_vs_buffer=layer->layer_vs_buffer;
            tmp_rho_buffer=layer->layer_rho_buffer;

            for(int i=0; i<numpoints; i++) { 
                lat_idx=lat_idx_buffer[i];
                lon_idx=lon_idx_buffer[i];
                offset= (lat_idx * nx) + lon_idx;

                data[i].vp = tmp_vp_buffer[offset];
                data[i].vs = tmp_vs_buffer[offset];
                data[i].rho =tmp_rho_buffer[offset];
            }
        } else {  
// a very special case,
            if( _ON && bucket_cnt < SJQBN_CACHE_LAYER_MAX) {	    
if(sjqbn_ucvm_debug){ fprintf(stderrfp,">> Using special cache layer\n"); }
if(sjqbn_ucvm_debug){ fprintf(stderrfp," BUCKET count is %d \n", bucket_cnt); }
                int last_idx=-1;
		sjqbn_cache_layer_t *last_layer=NULL;
		sjqbn_cache_layer_t *layer=NULL;
                for(int i=0; i<numpoints; i++) { 
                    lat_idx=lat_idx_buffer[i];
                    lon_idx=lon_idx_buffer[i];
                    dep_idx=dep_idx_buffer[i];
                    if(layer == NULL) {
		        layer=find_a_cache_layer(dataset, dep_idx);
                    } else if (last_idx == dep_idx) { 
                        layer = last_layer;
                    } else { // there is a layer switch
		        layer=find_a_cache_layer(dataset, dep_idx);
                    }
		    last_layer = layer;
		    last_idx=dep_idx;

                    offset= (lat_idx * nx) + lon_idx;
                    data[i].vp = (layer->layer_vp_buffer)[offset];
                    data[i].vs = (layer->layer_vs_buffer)[offset];
                    data[i].rho = (layer->layer_rho_buffer)[offset];
	        }
                } else {
// handle it as random and so just default to per location access
if(sjqbn_ucvm_debug){ fprintf(stderrfp,">> Using random call \n"); }
                    for(int i=0; i<numpoints; i++) { 
                        lat_idx=lat_idx_buffer[i];
                        lon_idx=lon_idx_buffer[i];
                        dep_idx=dep_idx_buffer[i];
                        data[i].vp=get_nc_vara_float(dataset->ncid, dataset->vp_varid, dep_idx, lat_idx, lon_idx);
                        data[i].vs=get_nc_vara_float(dataset->ncid, dataset->vs_varid, dep_idx, lat_idx, lon_idx);
                        data[i].rho=get_nc_vara_float(dataset->ncid, dataset->rho_varid, dep_idx, lat_idx, lon_idx);
	                }
            }
         }
    }

    return SUCCESS;
}

/**
 */
void sjqbn_setdebug() {
   sjqbn_ucvm_debug=1;
}               

/**
 * Called when the model is being discarded. Free all variables.
 *
 * @return SUCCESS
 */
int sjqbn_finalize() {

    sjqbn_is_initialized = 0;

    if (sjqbn_configuration) {
        sjqbn_configuration_finalize(sjqbn_configuration);
    }
    if (sjqbn_velocity_model) {
        sjqbn_velocity_model_finalize(sjqbn_velocity_model);
    }

    if(sjqbn_ucvm_debug) {
     fprintf(stderrfp,"DONE:\n"); 
     fclose(stderrfp);
    }

    return SUCCESS;
}

/**
 * Returns the version information.
 *
 * @param ver Version string to return.
 * @param len Maximum length of buffer.
 * @return Zero
 */
int sjqbn_version(char *ver, int len)
{
  int verlen;
  verlen = strlen(sjqbn_version_string);
  if (verlen > len - 1) {
    verlen = len - 1;
  }
  memset(ver, 0, len);
  strncpy(ver, sjqbn_version_string, verlen);
  return 0;
}

/**
 * Returns the model config information.
 *
 * @param key Config key string to return.
 * @param sz number of config terms.
 * @return Zero
 */
int sjqbn_config(char **config, int *sz)
{
  int len=strlen(sjqbn_config_string);
  if(len > 0) {
    *config=sjqbn_config_string;
    *sz=sjqbn_config_sz;
    return SUCCESS;
  }
  return FAIL;
}


/**
 * Reads the sjqbn_configuration file describing the various properties of SJQBN and populates
 * the sjqbn_configuration struct. This assumes sjqbn_configuration has been "calloc'ed" and validates
 * that each value is not zero at the end.
 *
 * @param file The sjqbn_configuration file location on disk to read.
 * @param config The sjqbn_configuration struct to which the data should be written.
 * @return Number of dataset expected in the model
 */
int sjqbn_read_configuration(char *file, sjqbn_configuration_t *config) {
    FILE *fp = fopen(file, "r");
    char key[40];
    char value[100];
    char line_holder[128];
    config->dataset_cnt=0;

    // If our file pointer is null, an error has occurred. Return fail.
    if (fp == NULL) { return UCVM_MODEL_CODE_ERROR; }

    // Read the lines in the sjqbn_configuration file.
    while (fgets(line_holder, sizeof(line_holder), fp) != NULL) {
        if (line_holder[0] != '#' && line_holder[0] != ' ' && line_holder[0] != '\n') {

         _splitline(line_holder, key, value);

            // Which variable are we editing?
            if (strcmp(key, "utm_zone") == 0) config->utm_zone = atoi(value);
            if (strcmp(key, "model_dir") == 0) sprintf(config->model_dir, "%s", value);
            if (strcmp(key, "interpolation") == 0) { 
                config->interpolation=0;
                if (strcmp(value,"on") == 0) config->interpolation=1;
            }
         /* for each dataset, allocate a model dataset's block and fill in */ 
            if (strcmp(key, "data_file") == 0) { 
                if( config->dataset_cnt < SJQBN_DATASET_MAX) {
              int rc=_setup_a_dataset(config,value);
                 if(rc == 1 ) { config->dataset_cnt++; }
                    } else {
                        sjqbn_print_error("Exceeded dataset maximum limit.");
                }
            }
        }
    }

    // Have we set up all sjqbn_configuration parameters?
    if (config->utm_zone == 0 || config->model_dir[0] == '\0' || config->dataset_cnt == 0 ) {
        sjqbn_print_error("One sjqbn_configuration parameter not specified. Please check your sjqbn_configuration file.");
        return UCVM_MODEL_CODE_ERROR;
    }

    fclose(fp);
    return UCVM_MODEL_CODE_SUCCESS;
}

/**
 * Extract sjqbn netcdf dataset specific info
 * and fill in the model info, one dataset at a time
 * and allocate required memory space
 *
 * @param 
 */
int _setup_a_dataset(sjqbn_configuration_t *config, char *blobstr) {
    /* parse the blob and grab the meta data */
    cJSON *confjson=cJSON_Parse(blobstr);
    int idx=config->dataset_cnt;

    /* grab the related netcdf file and extract dataset info */
    if(confjson == NULL) {
        const char *eptr = cJSON_GetErrorPtr();
        if(eptr != NULL) {
            if(sjqbn_ucvm_debug){ fprintf(stderrfp, "Configuration dataset setup error: (%s)\n", eptr); }
            return FAIL;
        }
    }

    cJSON *label = cJSON_GetObjectItemCaseSensitive(confjson, "LABEL");
    if(cJSON_IsString(label)){
        config->dataset_labels[idx]=strdup(label->valuestring);
    }
    cJSON *file = cJSON_GetObjectItemCaseSensitive(confjson, "FILE");
    if(cJSON_IsString(file)){
        config->dataset_files[idx]=strdup(file->valuestring);
    }

    return 1;
}

void _trimLast(char *str, char m) {
  int i;
  i = strlen(str);
  while (str[i-1] == m) {
    str[i-1] = '\0';
    i = i - 1;
  }
  return;
}

void _splitline(char* lptr, char key[], char value[]) {

  char *kptr, *vptr;

  for(int i=0; i<strlen(key); i++) { key[i]='\0'; }

  _trimLast(lptr,'\n');
  vptr = strchr(lptr, '=');
  int pos=vptr - lptr;

// skip space in key token from the back
  while ( lptr[pos-1]  == ' ') {
    pos--;
  }

  strncpy(key,lptr, pos);
  key[pos] = '\0';

  vptr++;
  while( vptr[0] == ' ' ) {
    vptr++;
  }
  strcpy(value,vptr);
  _trimLast(value,' ');
}


/*
 * setup the dataset's content 
 * and fill in the model info, one dataset at a time
 * and allocate required memory space
 *
 * */
int sjqbn_read_model(sjqbn_configuration_t *config, sjqbn_model_t *model, char *datadir) {

    int max_idx=model->dataset_cnt; // how many datasets are there
    for(int i=0; i<max_idx;i++) { 
        sjqbn_dataset_t *data=make_a_sjqbn_dataset(datadir, config->dataset_files[i], TooBig); 
// put into the velocity model
        model->datasets[i]=data;
    }
    return SUCCESS;
}

/**
 * Called to clear out the allocated memory 
 *
 * @param 
 */
int sjqbn_configuration_finalize(sjqbn_configuration_t *config) {
    int max_idx=config->dataset_cnt; // how many datasets are there
    for(int i=0; i<max_idx;i++) { 
        free(config->dataset_labels[i]);  
        free(config->dataset_files[i]);  
    }
    free(config);
    return SUCCESS;
}

/**
 * Called to clear out the allocated memory for model datasets 
 *
 * @param 
 */

int sjqbn_velocity_model_finalize(sjqbn_model_t *model) {
    int max_idx=model->dataset_cnt; // how many datasets are there
    for(int i=0; i<max_idx; i++) {
        sjqbn_dataset_t *data= model->datasets[i];
        if(data != NULL) {
          free_sjqbn_dataset(data);
        }
    }
    return SUCCESS;
}
int sjqbn_velocity_model_init(sjqbn_model_t *model) {
    model->dataset_cnt=0;
    for(int i=0; i<SJQBN_DATASET_MAX; i++) {
        model->datasets[i]=NULL;
    }
    return SUCCESS;
}


/**
 * Prints the error string provided.
 *
 * @param err The error string to print out to stderr.
 */
void sjqbn_print_error(char *err) {
    fprintf(stderr, "An error has occurred while executing sjqbn. The error was: \n\n%s\n",err);
    fprintf(stderr, "\nPlease contact software@scec.org and describe both the error and a bit\n");
    fprintf(stderr, "about the computer you are running sjqbn on (Linux, Mac, etc.).\n");
}

/*
 * Check if the data is too big to be loaded internally (exceed maximum
 * allowable by a INT variable)
 *
 */
static int too_big(sjqbn_dataset_t *dataset) {
    long max_size= (long) (dataset->nx) * dataset->ny * dataset->nz;
    long delta= max_size - INT_MAX;

    if( delta > 0) {
        return 1;
        } else {
            return 0;
        }
}

// The following functions are for dynamic library mode. If we are compiling
// a static library, these functions must be disabled to avoid conflicts.
#ifdef DYNAMIC_LIBRARY

/**
 * Init function loaded and called by the UCVM library. Calls sjqbn_init.
 *
 * @param dir The directory in which UCVM is installed.
 * @return Success or failure.
 */
int model_init(const char *dir, const char *label) {
    return sjqbn_init(dir, label);
}

/**
 * Query function loaded and called by the UCVM library. Calls sjqbn_query.
 *
 * @param points The basic_point_t array containing the points.
 * @param data The basic_properties_t array containing the material properties returned.
 * @param numpoints The number of points in the array.
 * @return Success or fail.
 */
int model_query(sjqbn_point_t *points, sjqbn_properties_t *data, int numpoints) {
    return sjqbn_query(points, data, numpoints);
}

/**
 * Finalize function loaded and called by the UCVM library. Calls sjqbn_finalize.
 *
 * @return Success
 */
int model_finalize() {
    return sjqbn_finalize();
}

/**
 * Version function loaded and called by the UCVM library. Calls sjqbn_version.
 *
 * @param ver Version string to return.
 * @param len Maximum length of buffer.
 * @return Zero
 */
int model_version(char *ver, int len) {
    return sjqbn_version(ver, len);
}

/**
 * Version function loaded and called by the UCVM library. Calls sjqbn_config.
 *
 * @param config Config string to return.
 * @param sz number of config terms
 * @return Zero
 */
int model_config(char **config, int *sz) {
    return sjqbn_config(config, sz);
}


int (*get_model_init())(const char *, const char *) {
        return &sjqbn_init;
}
int (*get_model_query())(sjqbn_point_t *, sjqbn_properties_t *, int) {
         return &sjqbn_query;
}
int (*get_model_finalize())() {
         return &sjqbn_finalize;
}
int (*get_model_version())(char *, int) {
         return &sjqbn_version;
}
int (*get_model_config())(char **, int*) {
    return &sjqbn_config;
}



#endif
