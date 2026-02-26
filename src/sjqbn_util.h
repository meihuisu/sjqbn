/**
 * @file sjqbn_util.h
 *
**/

#ifndef SJQBN_UTIL_H
#define SJQBN_UTIL_H

#define SJQBN_DATASET_MAX 10

#define SJQBN_CACHE_LAYER_MAX 20
#define SJQBN_CACHE_COL_MAX 10

typedef struct bucket_t{
    size_t value;   // the unique index
    size_t count;   // frequency
} bucket_t;

typedef struct sjqbn_cache_layer_t {
     int cache_layer_dep_idx;
     float *layer_vp_buffer; // nx * ny
     float *layer_vs_buffer;
     float *layer_rho_buffer;
} sjqbn_cache_layer_t;


typedef struct sjqbn_cache_col_t {
     int cache_col_lat_idx;
     int cache_col_lon_idx;
     float *col_vp_buffer; // nz
     float *col_vs_buffer;
     float *col_rho_buffer;
} sjqbn_cache_col_t;


/** The SJQBN a dataset's working structure. */
typedef struct sjqbn_dataset_t {
	/** tracking netcdf id **/
        int ncid;

	/** Number of x(lon) points */
	int nx;
	/** Number of y(lat) points */
	int ny;
	/** Number of z(dep) points */
	int nz;

	/** list of longitudes **/
	float *longitudes;
	/** list of latitudes **/
	float *latitudes;
	/** list of depths **/
	float *depths;

	int vp_varid;
	int vs_varid;
	int rho_varid;

	int elems;
	float *vp_buffer;
	float *vs_buffer;
	float *rho_buffer; 

	// track how many cached layers
        int layer_cache_cnt;
        sjqbn_cache_layer_t *layer_cache[SJQBN_CACHE_LAYER_MAX];

/* a cache of previous column from cache_depth_col_float call */
	int col_cache_cnt;
        sjqbn_cache_col_t *col_cache[SJQBN_CACHE_COL_MAX];

/* flag to show if data i read in memory */
        int in_memory;

} sjqbn_dataset_t;


/* utilitie functions */
sjqbn_dataset_t *make_a_sjqbn_dataset(char *datadir, char *datafile, int tooBig);
int free_sjqbn_dataset(sjqbn_dataset_t *data);
sjqbn_cache_col_t *find_a_cache_col(sjqbn_dataset_t *dataset, int target_lat_idx, int target_lon_idx);
void free_a_cache_col(sjqbn_cache_col_t *col);
sjqbn_cache_layer_t *find_a_cache_layer(sjqbn_dataset_t *dataset, int target_dep_idx);
void free_a_cache_layer(sjqbn_cache_layer_t *layer);

int bucket_an_array(size_t *idx_arr, size_t n);

#endif

