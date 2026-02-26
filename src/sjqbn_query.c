/*
 * @file sjqbn_query.c
 * @brief Bootstraps the test framework for the SJQBN library.
 * @author - SCEC
 * @version 1.0
 *
 * Tests the SJQBN library by loading it and executing the code as
 * UCVM would.
 *
 */

#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "ucvm_model_dtypes.h"
#include "sjqbn.h"

int sjqbn_debug=0;

int _compare_double(double f1, double f2) {
  double precision = 0.00001;
  if (((f1 - precision) < f2) && ((f1 + precision) > f2)) {
    return 1;
    } else {
      return 0;
  }
}

/* Usage function */
void usage() {
  printf("     sjqbn_query - (c) SCEC\n");
  printf("Extract velocities from a SJQBN\n");
  printf("\tusage: sjqbn_query [-d][-h] < file.in\n\n");
  printf("Flags:\n");
  printf("\t-d enable debug/verbose mode\n\n");
  printf("\t-h usage\n\n");
  printf("Output format is:\n");
  printf("\tvp vs rho\n\n");
  exit (0);
}

extern char *optarg;
extern int optind, opterr, optopt;

/**
 * Initializes and SJQBN in standalone mode with ucvm plugin 
 * api.
 *
 * @param argc The number of arguments.
 * @param argv The argument strings.
 * @return A zero value indicating success.
 */
int main(int argc, char* const argv[]) {

	// Declare the structures.
	sjqbn_point_t *pt;
	sjqbn_properties_t *ret;

	pt = malloc(10 * sizeof(sjqbn_point_t));
	ret = malloc(10 * sizeof(sjqbn_properties_t));

        int rc;
        int opt;


        /* Parse options */
        while ((opt = getopt(argc, argv, "dh")) != -1) {
          switch (opt) {
          case 'd':
            sjqbn_debug=1;
            break;
          case 'h':
            usage();
            exit(0);
            break;
          default: /* '?' */
            usage();
            exit(1);
          }
        }

        if(sjqbn_debug) { sjqbn_setdebug(); }

	// Initialize the model. 
        // try to use Use UCVM_INSTALL_PATH
        char *envstr=getenv("UCVM_INSTALL_PATH");
        if(envstr != NULL) {
	   assert(sjqbn_init(envstr, "sjqbn") == 0);
           } else {
	     assert(sjqbn_init("..", "sjqbn") == 0);
        }
	printf("Loaded the model successfully.\n");

        char line[1001];
        int done=0;
        int idx=0;

        while ( !done  && fgets(line, 1000, stdin) != NULL) {
           if(idx > 10) {
              done=1;
              continue;
           }
          
           if(sjqbn_debug) {
             fprintf(stderr,"LINE: %s\n",line);
           }

           if(line[0]=='#') continue; // comment line
           if (sscanf(line,"%lf %lf %lf",
                   &pt[idx].longitude,&pt[idx].latitude,&pt[idx].depth) == 3) {

              if(sjqbn_debug) {
                  fprintf(stderr, "calling : with %f,%f using > depth(%f)\n",
                         pt[idx].longitude,pt[idx].latitude,pt[idx].depth);
              }
              idx++;
           }
        }

        if(idx > 0) {
	   rc=sjqbn_query(pt, ret, idx);
           if(rc == 0) {
             for(int i=0; i<idx; i++) {
               printf("vs:%lf vp:%lf rho:%lf\n",ret[i].vs, ret[i].vp, ret[i].rho);
             }
           }
        }

	assert(sjqbn_finalize() == 0);
	printf("Model closed successfully.\n");

	return 0;
}
