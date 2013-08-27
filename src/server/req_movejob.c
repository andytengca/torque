/*
*         OpenPBS (Portable Batch System) v2.3 Software License
*
* Copyright (c) 1999-2000 Veridian Information Solutions, Inc.
* All rights reserved.
*
* ---------------------------------------------------------------------------
* For a license to use or redistribute the OpenPBS software under conditions
* other than those described below, or to purchase support for this software,
* please contact Veridian Systems, PBS Products Department ("Licensor") at:
*
*    www.OpenPBS.org  +1 650 967-4675                  sales@OpenPBS.org
*                        877 902-4PBS (US toll-free)
* ---------------------------------------------------------------------------
*
* This license covers use of the OpenPBS v2.3 software (the "Software") at
* your site or location, and, for certain users, redistribution of the
* Software to other sites and locations.  Use and redistribution of
* OpenPBS v2.3 in source and binary forms, with or without modification,
* are permitted provided that all of the following conditions are met.
* After December 31, 2001, only conditions 3-6 must be met:
*
* 1. Commercial and/or non-commercial use of the Software is permitted
*    provided a current software registration is on file at www.OpenPBS.org.
*    If use of this software contributes to a publication, product, or
*    service, proper attribution must be given; see www.OpenPBS.org/credit.html
*
* 2. Redistribution in any form is only permitted for non-commercial,
*    non-profit purposes.  There can be no charge for the Software or any
*    software incorporating the Software.  Further, there can be no
*    expectation of revenue generated as a consequence of redistributing
*    the Software.
*
* 3. Any Redistribution of source code must retain the above copyright notice
*    and the acknowledgment contained in paragraph 6, this list of conditions
*    and the disclaimer contained in paragraph 7.
*
* 4. Any Redistribution in binary form must reproduce the above copyright
*    notice and the acknowledgment contained in paragraph 6, this list of
*    conditions and the disclaimer contained in paragraph 7 in the
*    documentation and/or other materials provided with the distribution.
*
* 5. Redistributions in any form must be accompanied by information on how to
*    obtain complete source code for the OpenPBS software and any
*    modifications and/or additions to the OpenPBS software.  The source code
*    must either be included in the distribution or be available for no more
*    than the cost of distribution plus a nominal fee, and all modifications
*    and additions to the Software must be freely redistributable by any party
*    (including Licensor) without restriction.
*
* 6. All advertising materials mentioning features or use of the Software must
*    display the following acknowledgment:
*
*     "This product includes software developed by NASA Ames Research Center,
*     Lawrence Livermore National Laboratory, and Veridian Information
*     Solutions, Inc.
*     Visit www.OpenPBS.org for OpenPBS software support,
*     products, and information."
*
* 7. DISCLAIMER OF WARRANTY
*
* THIS SOFTWARE IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND. ANY EXPRESS
* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
* OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND NON-INFRINGEMENT
* ARE EXPRESSLY DISCLAIMED.
*
* IN NO EVENT SHALL VERIDIAN CORPORATION, ITS AFFILIATED COMPANIES, OR THE
* U.S. GOVERNMENT OR ANY OF ITS AGENCIES BE LIABLE FOR ANY DIRECT OR INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
* OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
* LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
* NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
* EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
* This license will be governed by the laws of the Commonwealth of Virginia,
* without reference to its choice of law rules.
*/
/*
 * req_movejob.c - function to move a job to another queue
 *
 * Included functions are:
 */

#include <pbs_config.h>   /* the master config generated by configure */

#include <stdio.h>
#include <sys/param.h>
#include "libpbs.h"
#include <errno.h>
#include <pthread.h>

#include "server_limits.h"
#include "list_link.h"
#include "attribute.h"
#include "credential.h"
#include "batch_request.h"
#include "pbs_job.h"
#include "log.h"
#include "../lib/Liblog/log_event.h"
#include "pbs_error.h"
#include "queue.h"
#include "ji_mutex.h"
#include "mutex_mgr.hpp"

/* Global Data Items: */

extern int LOGLEVEL;
extern char *msg_unkjobid;
extern char *msg_manager;
extern char *msg_movejob;
extern char *pbs_o_host;

int   svr_movejob(job *, char *, int *, struct batch_request *);
int   svr_chkque(job *, pbs_queue *, char *, int, char *);
job  *chk_job_request(char *, struct batch_request *);

/*
 * req_movejob = move a job to a new destination (local or remote)
 */

int req_movejob(

  batch_request *req) /* I */

  {
  job       *jobp;
  char       log_buf[LOCAL_LOG_BUF_SIZE];
  int        local_errno = 0;

  jobp = chk_job_request(req->rq_ind.rq_move.rq_jid, req);

  if (jobp == NULL)
    {
    return(PBSE_NONE);
    }

  mutex_mgr job_mutex(jobp->ji_mutex, true);

  if (LOGLEVEL >= 7)
    {
    sprintf(log_buf, "%s", jobp->ji_qs.ji_jobid);
    LOG_EVENT(PBSEVENT_JOB, PBS_EVENTCLASS_JOB, __func__, log_buf);
    }
  
  if ((jobp->ji_qs.ji_state != JOB_STATE_QUEUED) &&
      (jobp->ji_qs.ji_state != JOB_STATE_HELD) &&
      (jobp->ji_qs.ji_state != JOB_STATE_WAITING))
    {
#ifndef NDEBUG
    sprintf(log_buf, "%s %d %s", pbse_to_txt(PBSE_BADSTATE), jobp->ji_qs.ji_state, __func__);

    log_event(PBSEVENT_DEBUG,PBS_EVENTCLASS_JOB,jobp->ji_qs.ji_jobid,log_buf);
#endif /* NDEBUG */

    req_reject(PBSE_BADSTATE, 0, req, NULL, NULL);

    return(PBSE_NONE);
    }

  /*
   * svr_movejob() does the real work, handles both local and
   * network moves
   */
  
  /* We have found that sometimes the destination queue and the 
     parent queue are the same. If so we do not need to do
     anything else */
  if (strcmp(jobp->ji_qs.ji_queue, req->rq_ind.rq_move.rq_destin) == 0)
    {
    sprintf(log_buf, "Job %s already in queue %s", jobp->ji_qs.ji_jobid, jobp->ji_qs.ji_queue);
    if (LOGLEVEL >= 7)
      {
      log_event(PBSEVENT_JOB, PBS_EVENTCLASS_JOB, __func__, log_buf);
      }
    
    req_reject(PBSE_JOB_ALREADY_IN_QUEUE, 0, req, NULL, log_buf);
    return(PBSE_NONE);
    }

  switch (svr_movejob(jobp, req->rq_ind.rq_move.rq_destin, &local_errno, req))
    {

    case 0:

      /* success */
      snprintf(log_buf, sizeof(log_buf), "%s", msg_movejob);
      snprintf(log_buf + strlen(log_buf), sizeof(log_buf) - strlen(log_buf), msg_manager,
        req->rq_ind.rq_move.rq_destin, req->rq_user, req->rq_host);

      log_event(PBSEVENT_JOB,PBS_EVENTCLASS_JOB,jobp->ji_qs.ji_jobid,log_buf);

      reply_ack(req);

      break;

    case - 1:

    case 1:

      /* fail */

      /* NOTE:  can pass detailed response to requestor (NYI) */

      req_reject(local_errno, 0, req, NULL, NULL);

      break;

    case 2:

      /* deferred, will be handled by    */
      /* post_movejob() when the child completes */

      /* NO-OP */

      break;
    }  /* END switch (svr_movejob(jobp,req->rq_ind.rq_move.rq_destin,req)) */

  return(PBSE_NONE);
  }  /* END req_movejob() */






/*
 * req_orderjob = reorder the jobs in a queue
 */

int req_orderjob(

  struct batch_request *vp) /* I */

  {
  job                  *pjob;
  job                  *pjob1;
  job                  *pjob2;
  int                   rank;
  int                   rc = 0;
  char                  tmpqn[PBS_MAXQUEUENAME+1];
  struct batch_request *req = (struct batch_request *)vp;
  char                  log_buf[LOCAL_LOG_BUF_SIZE];
  pbs_queue            *pque1;
  pbs_queue            *pque2;
  bool                  reservation1 = false;
  bool                  reservation2 = false;

  if ((pjob1 = chk_job_request(req->rq_ind.rq_move.rq_jid, req)) == NULL)
    {
    return(PBSE_NONE);
    }

  mutex_mgr job1_mutex(pjob1->ji_mutex, true);

  if ((pjob2 = chk_job_request(req->rq_ind.rq_move.rq_destin, req)) == NULL)
    {
    return(PBSE_NONE);
    }

  mutex_mgr job2_mutex(pjob2->ji_mutex, true);

  if (((pjob = pjob1)->ji_qs.ji_state == JOB_STATE_RUNNING) ||
      ((pjob = pjob2)->ji_qs.ji_state == JOB_STATE_RUNNING))
    {
#ifndef NDEBUG
    sprintf(log_buf, "%s %d",
            pbse_to_txt(PBSE_BADSTATE),
            pjob->ji_qs.ji_state);

    strcat(log_buf, __func__);

    log_event(
      PBSEVENT_DEBUG,
      PBS_EVENTCLASS_JOB,
      pjob->ji_qs.ji_jobid,
      log_buf);
#endif /* NDEBUG */

    req_reject(PBSE_BADSTATE, 0, req, NULL, NULL);

    return(PBSE_NONE);
    }
  else if ((pjob1->ji_qhdr == NULL) || (pjob2->ji_qhdr == NULL))
    {
    req_reject(PBSE_BADSTATE, 0, req, NULL, "One of the jobs does not have a queue");
    return(PBSE_NONE);
    }
  else if (pjob1->ji_qhdr != pjob2->ji_qhdr)
    {
    /* jobs are in different queues */
    int ok = FALSE;

    if ((pque2 = get_jobs_queue(&pjob2)) == NULL)
      {
      rc = PBSE_BADSTATE;
      job2_mutex.set_lock_on_exit(false);
      }
    else
      {
      mutex_mgr pque2_mutex = mutex_mgr(pque2->qu_mutex, true);
      if ((rc = svr_chkque(pjob1, pque2, get_variable(pjob1, pbs_o_host), MOVE_TYPE_Order, NULL)) == PBSE_NONE)
        {
        reservation1 = have_reservation(pjob1, pque2);
        pque2_mutex.unlock();

        if ((pque1 = get_jobs_queue(&pjob1)) == NULL)
          {
          rc = PBSE_BADSTATE;
          job1_mutex.set_lock_on_exit(false);
          }
        else if (pjob1 != NULL)
          {
          mutex_mgr pque1_mutex = mutex_mgr(pque1->qu_mutex, true);
          if ((rc = svr_chkque(pjob2, pque1, get_variable(pjob2, pbs_o_host), MOVE_TYPE_Order, NULL)) == PBSE_NONE)
            {
            reservation2 = have_reservation(pjob2, pque1);
            ok = TRUE;
            }
          }
        }
      }

    if (ok == FALSE)
      {
      req_reject(rc, 0, req, NULL, NULL);

      return(PBSE_NONE);
      }
    }

  /* now swap the order of the two jobs in the queue lists */
  rank = pjob1->ji_wattr[JOB_ATR_qrank].at_val.at_long;

  pjob1->ji_wattr[JOB_ATR_qrank].at_val.at_long =
    pjob2->ji_wattr[JOB_ATR_qrank].at_val.at_long;

  pjob2->ji_wattr[JOB_ATR_qrank].at_val.at_long = rank;

  if (pjob1->ji_qhdr != pjob2->ji_qhdr)
    {
    strcpy(tmpqn, pjob1->ji_qs.ji_queue);
    strcpy(pjob1->ji_qs.ji_queue, pjob2->ji_qs.ji_queue);
    strcpy(pjob2->ji_qs.ji_queue, tmpqn);

    svr_dequejob(pjob1, FALSE);
    svr_dequejob(pjob2, FALSE);

    if (svr_enquejob(pjob1, FALSE, -1, reservation1) == PBSE_JOB_RECYCLED)
      {
      pjob1 = NULL;
      job1_mutex.set_lock_on_exit(false);
      }

    if (svr_enquejob(pjob2, FALSE, -1, reservation2) == PBSE_JOB_RECYCLED)
      {
      pjob2 = NULL;
      job2_mutex.set_lock_on_exit(false);
      }
    }
  else
    {
    if ((pque1 = get_jobs_queue(&pjob1)) != NULL)
      {
      mutex_mgr pque1_mutex = mutex_mgr(pque1->qu_mutex, true);
      swap_jobs(pque1->qu_jobs,pjob1,pjob2);
      swap_jobs(NULL,pjob1,pjob2);
      }
    }

  /* need to update disk copy of both jobs to save new order */
  if (pjob1 != NULL)
    {
    job_save(pjob1, SAVEJOB_FULL, 0);
    }

  if (pjob2 != NULL)
    {
    job_save(pjob2, SAVEJOB_FULL, 0);
    }

  /* SUCCESS */
  reply_ack(req);

  return(PBSE_NONE);
  }  /* END req_orderjob() */

