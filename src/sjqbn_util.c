/**
         sjqbn_util.c
**/

#include "ucvm_model_dtypes.h"
#include "sjqbn.h"
#include "um_netcdf.h"

#include "sjqbn_util.h"

/**** for sjqbn_dataset_t ****/
sjqbn_dataset_t *make_a_sjqbn_dataset(char *datadir, char *datafile, int tooBig) {

    char filepath[256];
    size_t nelems= 0;
    nc_type vtype;

    sjqbn_dataset_t *data=(sjqbn_dataset_t *)malloc(sizeof(sjqbn_dataset_t));

    sprintf(filepath, "%s/%s", datadir, datafile);
    if(sjqbn_ucvm_debug) fprintf(stderrfp," data file ..%s\n", filepath);

/* setup ncid */
    data->ncid=open_nc(filepath);
  
/* setup nx/ny/nz and void ptrs */
    data->longitudes=(float *) get_nc_buffer(data->ncid, "longitude", filepath, &vtype, &nelems, 1);
    data->nx=nelems;
    if(sjqbn_ucvm_debug) {
        fprintf(stderrfp, "  Longitudes: %d\n", nelems);
        for(int i=0;i<nelems; i++) {
            fprintf(stderrfp, "%d  %f\n", i, data->longitudes[i]);
       	}
    }

    data->latitudes=(float *) get_nc_buffer(data->ncid, "latitude", filepath, &vtype, &nelems, 1);
    data->ny=nelems;
    if(sjqbn_ucvm_debug) {
        fprintf(stderrfp, "  Latitude: %d\n", nelems);
        for(int i=0;i<nelems; i++) {
            fprintf(stderrfp, "%d  %f\n", i, data->latitudes[i]);
       	}
    }

    data->depths=(float *) get_nc_buffer(data->ncid, "depth", filepath, &vtype, &nelems, 1);
    data->nz=nelems;
    if(sjqbn_ucvm_debug) {
        fprintf(stderrfp, "  Depths: %d\n", nelems);
        for(int i=0;i<nelems; i++) {
            fprintf(stderrfp, "%d  %f\n", i, data->depths[i]);
       	}
    }

/* Get variable ID by name */
    data->vp_varid=get_nc_varid(data->ncid,"vp",filepath);
    data->vs_varid=get_nc_varid(data->ncid,"vs",filepath);
    data->rho_varid=get_nc_varid(data->ncid,"rho",filepath);

    data->layer_cache_cnt=0;
    data->col_cache_cnt=0;

    data->in_memory =0;

    if(!tooBig) {
/* load all vp/vs/rho data in memory */
        int total= data->nx * data->ny * data->nz;

        data->vp_buffer = (float *)malloc(total * sizeof(float));
        data->vs_buffer = (float *)malloc(total * sizeof(float));
        data->rho_buffer = (float *)malloc(total * sizeof(float));

        data->vp_buffer=get_nc_buffer(data->ncid, "vp", filepath, &vtype, &nelems, 3);
        data->vs_buffer=get_nc_buffer(data->ncid, "vs", filepath, &vtype, &nelems, 3);
        data->rho_buffer=get_nc_buffer(data->ncid, "rho", filepath, &vtype, &nelems, 3);

	data->elems=total;
        data->in_memory=1;

        } else {
	  data->elems=0;
	  data->vp_buffer=NULL;
	  data->vs_buffer=NULL;
	  data->rho_buffer=NULL;
    }

    return data;
}


int free_sjqbn_dataset(sjqbn_dataset_t *data) {
    free(data->depths);
    free(data->latitudes);
    free(data->longitudes);
    if(data->vp_buffer != NULL) free(data->vp_buffer);
    if(data->vs_buffer != NULL) free(data->vs_buffer);
    if(data->rho_buffer != NULL) free(data->rho_buffer);
    nc_close(data->ncid);

    //free the caches
    for(int i=0; i< data->layer_cache_cnt; i++) {
      free_a_cache_layer(data->layer_cache[i]);
    }
    for(int i=0; i< data->col_cache_cnt; i++) {
      free_a_cache_col(data->col_cache[i]);
    }

    free(data);
    return SUCCESS;
}

/**** for sjqbn_cache_col_t ****/
sjqbn_cache_col_t *_add_a_cache_col(sjqbn_dataset_t *dataset, int target_lat_idx, int target_lon_idx) {
 
   int nz=dataset->nz;

   sjqbn_cache_col_t *col= (sjqbn_cache_col_t *) malloc(sizeof(sjqbn_cache_col_t));

   col->cache_col_lat_idx=target_lat_idx;
   col->cache_col_lon_idx=target_lon_idx;

   // alloc space first 
   col->col_vp_buffer=(float *) malloc(nz * sizeof(float));
   col->col_vs_buffer=(float *) malloc(nz * sizeof(float));
   col->col_rho_buffer=(float *) malloc(nz * sizeof(float));

   cache_depth_col_float(dataset->ncid, dataset->vp_varid,
                        nz, target_lat_idx, target_lon_idx, col->col_vp_buffer);
   cache_depth_col_float(dataset->ncid, dataset->vs_varid,
                        nz, target_lat_idx, target_lon_idx, col->col_vs_buffer);
   cache_depth_col_float(dataset->ncid, dataset->rho_varid,
                        nz, target_lat_idx, target_lon_idx, col->col_rho_buffer);
   return col;
}

sjqbn_cache_col_t *find_a_cache_col(sjqbn_dataset_t *dataset, int target_lat_idx, int target_lon_idx) {

   int cnt= dataset->col_cache_cnt; 
   sjqbn_cache_col_t *col;

   for(int i=0; i< cnt; i++) {
     col=dataset->col_cache[i];
     if((col->cache_col_lat_idx == target_lat_idx) &&
		      (col->cache_col_lon_idx == target_lon_idx) ) {
        // found it
        return col;
     }
   }
   // load it from the netcdf file
   col=_add_a_cache_col(dataset, target_lat_idx, target_lon_idx);
    
   // find a space to put in (in case it is full)
   if( cnt < SJQBN_CACHE_COL_MAX) {
       dataset->col_cache[cnt]=col;
       dataset->col_cache_cnt=cnt+1;
       } else {
// else has to free one out
         int use_idx=(cnt+1) % SJQBN_CACHE_COL_MAX;
         free_a_cache_col(dataset->col_cache[use_idx]);
         dataset->col_cache[use_idx]=col;

   }

   return col;     
}

void free_a_cache_col(sjqbn_cache_col_t *col) {

   // free buffer first 
   free(col->col_vp_buffer);
   free(col->col_vs_buffer);
   free(col->col_rho_buffer);

   free(col);
}

/**** for sjqbn_cache_layer_t ****/
sjqbn_cache_layer_t *_add_a_cache_layer(sjqbn_dataset_t *dataset, int target_dep_idx) {

    int nx=dataset->nx;
    int ny=dataset->ny;

    if(sjqbn_ucvm_debug) { fprintf(stderrfp, "  Loading a new layer: %zu\n", target_dep_idx); }
    sjqbn_cache_layer_t *layer= (sjqbn_cache_layer_t *) malloc(sizeof(sjqbn_cache_layer_t));

    layer->cache_layer_dep_idx=target_dep_idx;

    layer->layer_vp_buffer = (float * )malloc((nx*ny) * sizeof(float));
    layer->layer_vs_buffer = (float * )malloc((nx*ny) * sizeof(float));
    layer->layer_rho_buffer = (float * )malloc((nx*ny) * sizeof(float));

    cache_latlon_layer_float(dataset->ncid, dataset->vp_varid,
                      target_dep_idx, ny, nx, layer->layer_vp_buffer);
    cache_latlon_layer_float(dataset->ncid, dataset->vs_varid,
                      target_dep_idx, ny, nx, layer->layer_vs_buffer);
    cache_latlon_layer_float(dataset->ncid, dataset->rho_varid,
                      target_dep_idx, ny, nx, layer->layer_rho_buffer);
    return layer;
}

sjqbn_cache_layer_t *find_a_cache_layer(sjqbn_dataset_t *dataset, int target_dep_idx) {

   int cnt= dataset->layer_cache_cnt;
   sjqbn_cache_layer_t *layer;

   for(int i=0; i< cnt; i++) {
     layer=dataset->layer_cache[i];
     if((layer->cache_layer_dep_idx == target_dep_idx) ) {
        // found it
        return layer;
     }
   }
   // load it from the netcdf file
   layer=_add_a_cache_layer(dataset, target_dep_idx);

   // find a space to put in (in case it is full)
   if( cnt < SJQBN_CACHE_LAYER_MAX) {
       dataset->layer_cache[cnt]=layer;
       dataset->layer_cache_cnt=cnt+1;
       } else {
// else has to free one out
         int use_idx=(cnt+1) % SJQBN_CACHE_LAYER_MAX;
         free_a_cache_layer(dataset->layer_cache[use_idx]);
         dataset->layer_cache[use_idx]=layer;
   }
   return layer;     
}

void free_a_cache_layer(sjqbn_cache_layer_t *layer) {

   free(layer->layer_vp_buffer);
   free(layer->layer_vs_buffer);
   free(layer->layer_rho_buffer);

   free(layer);
}

/*** bucket sort the layer index ***/

static int cmp_size_t(const void *a, const void *b) {
    size_t av = *(const size_t *)a;
    size_t bv = *(const size_t *)b;
    return (av > bv) - (av < bv);
}

/**
 * Groups identical values without modifying the input array.
 *
 * @param arr  pointer to input values (not modified)
 * @param n    number of elements in arr
 * @param out_bucket_count  output: number of unique buckets
 * @return dynamically-allocated array of Bucket (size = *out_bucket_count), or NULL on error
 *
 * Notes:
 *   - On success with n==0: returns NULL and sets *out_bucket_count = 0
 *   - Caller must free() the returned pointer.
 */
bucket_t *bucketize_unique_counts(const size_t *arr, size_t n, size_t *out_bucket_count) {
    if (!out_bucket_count) return NULL;
    *out_bucket_count = 0;

    if (!arr || n == 0) {
        return NULL; // nothing to do
    }

    // 1) Copy input (to keep original unmodified)
    size_t *copy = malloc(n * sizeof(*copy));
    if (!copy) return NULL;
    memcpy(copy, arr, n * sizeof(*copy));

    // 2) Sort the copy
    qsort(copy, n, sizeof(*copy), cmp_size_t);

    // 3) Allocate worst-case buckets (all values unique)
    bucket_t *buckets = malloc(n * sizeof(*buckets));
    if (!buckets) {
        free(copy);
        return NULL;
    }

    // 4) Scan sorted copy to form (value, count) buckets
    size_t bcount = 0;
    size_t i = 0;
    while (i < n) {
        size_t v = copy[i];
        size_t j = i + 1;
        while (j < n && copy[j] == v) j++;

        buckets[bcount].value = v;
        buckets[bcount].count = j - i;
        bcount++;

        i = j;
    }

    // 5) (Optional) shrink to fit
    bucket_t *shrunk = realloc(buckets, bcount * sizeof(*buckets));
    if (shrunk) buckets = shrunk;

    free(copy);
    *out_bucket_count = bcount;
    return buckets;
}

int bucket_an_array(size_t *idx_arr, size_t n) { 

    size_t bucket_count = 0;
    bucket_t *buckets = bucketize_unique_counts(idx_arr, n, &bucket_count);
    if (!buckets && bucket_count != 0) {
        fprintf(stderr, "Error creating buckets\n");
    }

    if(sjqbn_ucvm_debug) { fprintf(stderrfp, "Unique buckets: %zu\n", bucket_count); }
    for (size_t i = 0; i < bucket_count; ++i) {
        if(sjqbn_ucvm_debug) { fprintf(stderrfp, "value=%zu count=%zu\n", buckets[i].value, buckets[i].count); }
    }

    free(buckets);
    return bucket_count;
}

