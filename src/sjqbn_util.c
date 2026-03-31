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

    data->in_memory =0;

/* load all vp/vs/rho data in memory */
    int total= data->nx * data->ny * data->nz;

    data->vp_buffer = (float *)malloc(total * sizeof(float));
    data->vs_buffer = (float *)malloc(total * sizeof(float));
    data->rho_buffer = (float *)malloc(total * sizeof(float));

    data->vp_buffer=get_nc_float_buffer(data->ncid, "vp", filepath, &vtype, &nelems, 3);
    data->vs_buffer=get_nc_float_buffer(data->ncid, "vs", filepath, &vtype, &nelems, 3);
    data->rho_buffer=get_nc_float_buffer(data->ncid, "rho", filepath, &vtype, &nelems, 3);

    data->elems=total;
    data->in_memory=1;

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

    free(data);
    return SUCCESS;
}

/**** straight or trilinear/bilinear ****/
int _buffer_offset(sjqbn_dataset_t * dataset, int x_idx, int  y_idx, int z_idx) {
    int nx=dataset->nx;
    int ny=dataset->ny;
    int nz=dataset->nz;

    int offset= (z_idx)*(ny * nx)+(y_idx)*(nx)+x_idx;
    if(sjqbn_ucvm_debug) { fprintf(stderrfp,"\nTarget offset %d : idx lon/lat/dep = %d/%d/%d\n", offset,x_idx, y_idx, z_idx); }

    return offset;
}

int get_one_property(sjqbn_dataset_t *dataset, sjqbn_pt_info_t *pt, sjqbn_properties_t *data) {
    int offset= _buffer_offset(dataset, pt->lon_idx, pt->lat_idx, pt->dep_idx);

    data->vp=dataset->vp_buffer[offset];
    data->vs=dataset->vs_buffer[offset];
    data->rho=dataset->rho_buffer[offset];
    return offset;
}


float _interp_a_point(sjqbn_dataset_t *dataset, float *buffer, sjqbn_pt_info_t *pt) {
    int lon_idx=pt->lon_idx;
    int lat_idx=pt->lat_idx;
    int dep_idx=pt->dep_idx;
    float lon_percent=pt->lon_percent;
    float lat_percent=pt->lat_percent;
    float dep_percent=pt->dep_percent;

    if(pt->lon_idx < 0 || pt->lat_idx < 0 || pt->dep_idx < 0 ||
         pt->lon_idx +1 >= dataset->nx || pt->lat_idx +1 >= dataset->ny || pt->dep_idx+1 >= dataset->nz ) {
        // out of bound
        return -1;
    }

    float val0= buffer[_buffer_offset(dataset,lon_idx,lat_idx,dep_idx)];      // x,    y, z
    float val1= buffer[_buffer_offset(dataset,lon_idx+1,lat_idx,dep_idx)];    // x+1,  y, z 
    float val2= buffer[_buffer_offset(dataset,lon_idx,lat_idx+1,dep_idx)];    // x,  y+1, z 
    float val3= buffer[_buffer_offset(dataset,lon_idx+1,lat_idx+1,dep_idx)];  // x+1,y+1, z
    float val4= buffer[_buffer_offset(dataset,lon_idx,lat_idx,dep_idx+1)];    // x,    y, z+1
    float val5= buffer[_buffer_offset(dataset,lon_idx+1,lat_idx,dep_idx+1)];  // x+1,  y, z+1
    float val6= buffer[_buffer_offset(dataset,lon_idx,lat_idx+1,dep_idx+1)];  // x,  y+1, z+1
    float val7= buffer[_buffer_offset(dataset,lon_idx+1,lat_idx+1,dep_idx+1)];// x+1,y+1, z+1
        
    float val00= val0 * (1-lon_percent) + val1 * lon_percent;    
    float val11= val4 * (1-lon_percent) + val5 * lon_percent;    
    float val22= val2 * (1-lon_percent) + val3 * lon_percent;    
    float val33= val6 * (1-lon_percent) + val7 * lon_percent;    

    float val000 = val00 * (1-lat_percent) + val22 * lat_percent;
    float val111 = val11 * (1-lat_percent) + val33 * lat_percent;

    float val0000 = val000 * (1-dep_percent) + val111 * dep_percent;
    return val0000;
}

void get_interp_property(sjqbn_dataset_t *dataset, sjqbn_pt_info_t *pt, sjqbn_properties_t *data) {

    if(sjqbn_ucvm_debug) { fprintf(stderrfp,"\nInterp PROCESSING for vp\n"); }
    data->vp = _interp_a_point(dataset, dataset->vp_buffer, pt);
    if(sjqbn_ucvm_debug) { fprintf(stderrfp,"\nInterp PROCESSING for vs\n"); }
    data->vs = _interp_a_point(dataset, dataset->vs_buffer, pt);
    if(sjqbn_ucvm_debug) { fprintf(stderrfp,"\nInterp PROCESSING for rho\n"); }
    data->rho = _interp_a_point(dataset, dataset->rho_buffer, pt);
    return;
}

