#include "license_pbs.h" /* See here for the software license */
#include <stdlib.h>
#include <stdio.h>

#include "attribute.h"
#include "pbs_job.h"

task::~task() {}
attribute_def job_attr_def[100];

void nodes_free(mom_job *pj) {}

int csv_length(const char *csv_str)
  {
  return(0);
  }

char *csv_nth(const char *csv_str, int n)
  {
  return(NULL);
  }

char* remove_from_csv(

  char* src,            /* I - line with csv values*/
  char* model_pattern   /* I - pattern with models*/)
  {
  return(NULL);
  }

char *csv_find_string(
    
  const char *csv_str,
  const char *search_str)

  {
  return(NULL);
  }

int ctnodes(

  const char *spec)

  {
  return(0);
  }

