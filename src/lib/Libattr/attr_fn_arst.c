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
#include <pbs_config.h>   /* the master config generated by configure */

#include <assert.h>
#include <ctype.h>
#include <memory.h>
#ifndef NDEBUG
#include <stdio.h>
#endif
#include <stdlib.h>
#include <string.h>
#include "pbs_ifl.h"
#include "list_link.h"
#include "attribute.h"
#include "pbs_error.h"

/*
 * This file contains general function for attributes of type
 * array of (pointers to) strings
 *
 * Each set has functions for:
 * Decoding the value string to the machine representation.
 * Encoding the internal representation of the attribute to external
 * Setting the value by =, + or - operators.
 * Comparing a (decoded) value with the attribute value.
 * Freeing the space malloc-ed to the attribute value.
 *
 * Some or all of the functions for an attribute type may be shared with
 * other attribute types.
 *
 * The prototypes are declared in "attribute.h"
 *
 * ----------------------------------------------------------------------------
 * Attribute functions for attributes with value type "array of strings".
 *
 * The "encoded" or external form of the value is a string with the orginial
 * strings separated by commas (or new-lines) and terminated by a null.
 * Any embedded commas or black-slashes must be escaped by a prefixed back-
 * slash.
 *
 * The "decoded" form is a set of strings pointed to by an array_strings struct
 * ----------------------------------------------------------------------------
 */



/**
 * Parse string and convert to attribute array.
 *
 * @return 0 on SUCCESS, PBSE_* on FAILURE
 */

int decode_arst_direct(

  struct attribute *patr,  /* I (modified) */
  char             *val)   /* I */

  {
  unsigned long bksize;
  int        j;
  int        ns;
  char      *pbuf;
  char      *pc;
  char      *pstr;
  size_t     ssize;
  char      *tmpval;

  struct array_strings *stp;

  /*
   * determine number of sub strings, each sub string is terminated
   * by a non-escaped comma or a new-line, the whole string is terminated
   * by a null
   */

  ns = 1;
  ssize = strlen(val) + 1;

  if ((tmpval = malloc((unsigned)ssize)) == NULL)
    {
    /* FAILURE */

    return(PBSE_SYSTEM);
    }

  strcpy(tmpval,val);

  for (pc = tmpval;*pc;pc++)
    {
    if (*pc == '\\')
      {
      if (*++pc == '\0')
        break;
      }
    else if ((*pc == ',') || (*pc == '\n'))
      {
      ++ns;
      }
    }

  pc--;

  if (((*pc == '\n') || (*pc == ',')) && (*(pc - 1) != '\\'))
    {
    ns--;  /* strip any trailing null string */
    *pc = '\0';
    ssize--;
    }

  if ((pbuf = malloc((unsigned)ssize)) == NULL)
    {
    /* FAILURE */

    return(PBSE_SYSTEM);
    }

  bksize = (ns - 1) * sizeof(char *) + sizeof(struct array_strings);

  if ((stp = (struct array_strings *)malloc(bksize)) == NULL)
    {
    /* FAILURE */

    return(PBSE_SYSTEM);
    }

  stp->as_npointers = ns;

  /* number of slots (sub strings) */
  /* for the strings themselves */

  stp->as_buf     = pbuf;
  stp->as_next    = pbuf;
  stp->as_bufsize = (unsigned int)ssize;

  /* now copy in substrings and set pointers */

  pc = pbuf;

  j  = 0;

  pstr = parse_comma_string(tmpval);

  while ((pstr != NULL) && (j < ns))
    {
    stp->as_string[j] = pc;

    while (*pstr)
      {
      if (*pstr == '\\')
        if (*++pstr == '\0')
          break;

      *pc++ = *pstr++;
      }

    *pc++ = '\0';

    j++;

    pstr = parse_comma_string(NULL);
    }  /* END while ((pstr != NULL) && (j < ns)) */

  stp->as_usedptr = j;

  stp->as_next    = pc;

  patr->at_flags |= ATR_VFLAG_SET | ATR_VFLAG_MODIFY;

  patr->at_val.at_arst = stp;

  free(tmpval);
 
  /* SUCCESS */

  return(0);
  }  /* END decode_arst_direct() */





/*
 * decode_arst - decode a comma string into an attr of type ATR_TYPE_ARST
 *
 * @see decode_arst_direct() - child
 * @see set_arst() - child
 *
 * Returns: 0 if ok,
 *  >0 error number1 if error,
 *  *patr members set
 */

int decode_arst(

  struct attribute *patr,    /* O (modified) */
  char             *name,    /* I attribute name (notused) */
  char             *rescn,   /* I resource name (notused) */
  char             *val)     /* I attribute value */

  {
  int   rc;
  attribute temp;

  if ((val == NULL) || (strlen(val) == 0))
    {
    free_arst(patr);

    patr->at_flags &= ~ATR_VFLAG_MODIFY; /* _SET cleared in free_arst */

    return(0);
    }

  if ((patr->at_flags & ATR_VFLAG_SET) && (patr->at_val.at_arst))
    {
    /* already have values, decode new into temp */
    /* then use set(incr) to add new to existing */

    temp.at_val.at_arst = NULL;

    /* convert value string into attribute array */

    if ((rc = decode_arst_direct(&temp,val)) != 0)
      {
      /* FAILURE */

      return(rc);
      }

    rc = set_arst(patr,&temp,INCR);

    free_arst(&temp);

    return(rc);
    }

  /* decode directly into real attribute */

  return(decode_arst_direct(patr,val));
  }  /* END decode_arst() */





/*
 * decode_arst_merge - decode a comma string into an attr of type ATR_TYPE_ARST
 *
 * @see decode_arst_direct() - child
 * @see set_arst() - child
 *
 * Returns: 0 if ok,
 *  >0 error number1 if error,
 *  *patr members set
 */

int decode_arst_merge(

  struct attribute *patr,    /* O (modified) */
  char             *name,    /* I attribute name (notused) */
  char             *rescn,   /* I resource name (notused) */
  char             *val)     /* I attribute value */

  {
  int       rc;
  attribute new;
  attribute tmp;

  if ((val == NULL) || (strlen(val) == 0))
    {
    free_arst(patr);

    patr->at_flags &= ~ATR_VFLAG_MODIFY; /* _SET cleared in free_arst */

    return(0);
    }

  if (!(patr->at_flags & ATR_VFLAG_SET) || (patr->at_val.at_arst == NULL))
    {
    /* decode directly into real attribute */

    return(decode_arst_direct(patr,val));
    }

  memset(&new,0x0,sizeof(attribute));
  memset(&tmp,0x0,sizeof(attribute));

  /* already have values, decode new into temp */
  /* then use set(incr) to add new to existing */

  /* convert value string into attribute array */

  if ((rc = decode_arst_direct(&new,val)) != 0)
    {
    /* FAILURE */

    return(rc);
    }

  /* copy patr to temp, and new to patr */

  tmp.at_val.at_arst = patr->at_val.at_arst;
  patr->at_val.at_arst = new.at_val.at_arst;

  /* incr original patr value onto new patr */

  tmp.at_flags |= ATR_VFLAG_SET;
  rc = set_arst(patr,&tmp,MERGE);

  free_arst(&tmp);

  return(rc);
  }  /* END decode_arst_merge() */





    
/*
 * encode_arst - encode attr of type ATR_TYPE_ARST into attrlist entry
 *
 * Mode ATR_ENCODE_CLIENT - encode strings into single super string
 *       separated by ','
 *
 * Mode ATR_ENCODE_SVR    - treated as above
 *
 * Mode ATR_ENCODE_MOM    - treated as above
 *
 * Mode ATR_ENCODE_SAVE - encode strings into single super string
 *     separated by '\n'
 *
 * Returns: >0 if ok, entry created and linked into list
 *   =0 no value to encode, entry not created
 *   -1 if error
 */





/*ARGSUSED*/

int encode_arst(

  attribute  *attr,   /* I ptr to attribute to encode */
  tlist_head *phead,  /* O ptr to head of attrlist list */
  char       *atname, /* I attribute name */
  char       *rsname, /* I resource name or NULL (optional) */
  int         mode)   /* I encode mode */

  {
  char  *end;
  int   i;
  int   j;
  svrattrl *pal;
  char  *pc;
  char   *pfrom;
  char   separator;

  if (attr == NULL)
    {
    /* FAILURE - invalid parameters */

    return(-2);
    }

  if (((attr->at_flags & ATR_VFLAG_SET) == 0) ||
      !attr->at_val.at_arst ||
      !attr->at_val.at_arst->as_usedptr)
    {
    /* SUCCESS - empty list */

    return(0); /* no values */
    }

  i = (int)(attr->at_val.at_arst->as_next - attr->at_val.at_arst->as_buf);

  if (mode == ATR_ENCODE_SAVE)
    {
    separator = '\n'; /* new-line for encode_acl  */

    /* allow one extra byte for final new-line */

    j = i + 1;
    }
  else
    {
    separator = ','; /* normally a comma is the separator */

    j = i;
    }

  /* how many back-slashes are required */

  for (pc = attr->at_val.at_arst->as_buf;pc < attr->at_val.at_arst->as_next;++pc)
    {
    if ((*pc == '"') || (*pc == '\'') || (*pc == ',') || (*pc == '\\') || (*pc == '\n'))
      ++j;
    }

  pal = attrlist_create(atname, rsname, j);

  if (pal == NULL)
    {
    return(-1);
    }

  pal->al_flags = attr->at_flags;

  pc    = pal->al_value;
  pfrom = attr->at_val.at_arst->as_buf;

  /* replace nulls between sub-strings with separater characters */
  /* escape any embedded special character */

  end = attr->at_val.at_arst->as_next;

  while (pfrom < end)
    {
    if ((*pfrom == '"') || (*pfrom == '\'') || (*pfrom == ',') || (*pfrom == '\\') || (*pfrom == '\n'))
      {
      *pc++ = '\\';
      *pc   = *pfrom;
      }
    else if (*pfrom == '\0')
      {
      *pc = separator;
      }
    else
      {
      *pc = *pfrom;
      }

    pc++;

    pfrom++;
    }

  /* convert the last null to separator only if going to new-lines */

  if (mode == ATR_ENCODE_SAVE)
    *pc = '\0'; /* insure string terminator */
  else
    *(pc - 1) = '\0';

  append_link(phead, &pal->al_link, pal);

  return(1);
  }  /* END encode_arst() */





/*
 * set_arst - set value of attribute of type ATR_TYPE_ARST to another
 *
 * A=B --> set of strings in A replaced by set of strings in B
 * A+B --> set of strings in B appended to set of strings in A
 * A-B --> any string in B found is A is removed from A
 *
 * Returns: 0 if ok
 *  >0 if error
 */

int set_arst(

  struct attribute *attr,  /* I/O */
  struct attribute *new,   /* I */
  enum batch_op     op)    /* I */

  {
  int  i;
  int  j;
  int   nsize;
  int   need;
  long  offset;
  char *pc;
  long  used;

  struct array_strings *newpas;

  struct array_strings *pas;

  assert(attr && new && (new->at_flags & ATR_VFLAG_SET));

  pas = attr->at_val.at_arst;
  newpas = new->at_val.at_arst;

  if (newpas == NULL)
    {
    return(PBSE_INTERNAL);
    }

  if (pas == NULL)
    {
    /* not array_strings control structure, make one */

    j = newpas->as_npointers;

    if (j < 1)
      {
      return(PBSE_INTERNAL);
      }

    need = sizeof(struct array_strings) + (j - 1) * sizeof(char *);

    pas = (struct array_strings *)malloc((size_t)need);

    if (pas == NULL)
      {
      return(PBSE_SYSTEM);
      }

    pas->as_npointers = j;

    pas->as_usedptr = 0;
    pas->as_bufsize = 0;
    pas->as_buf     = NULL;
    pas->as_next    = NULL;
    attr->at_val.at_arst = pas;
    }  /* END if (pas == NULL) */

  if ((op == INCR) && !pas->as_buf)
    op = SET; /* no current strings, change op to SET */

  /*
   * At this point we know we have a array_strings struct initialized
   */

  switch (op)
    {
    case SET:

      /*
       * Replace old array of strings with new array, this is
       * simply done by deleting old strings and appending the
       * new (to the null set).
       */

      for (i = 0;i < pas->as_usedptr;i++)
        pas->as_string[i] = NULL; /* clear all pointers */

      pas->as_usedptr = 0;

      pas->as_next    = pas->as_buf;

      if (new->at_val.at_arst == (struct array_strings *)0)
        break; /* none to set */

      nsize = newpas->as_next - newpas->as_buf; /* space needed */

      if (nsize > pas->as_bufsize)
        {
        /* new data won't fit */

        if (pas->as_buf)
          free(pas->as_buf);

        nsize += nsize / 2;   /* alloc extra space */

        if (!(pas->as_buf = malloc((size_t)nsize)))
          {
          pas->as_bufsize = 0;

          return(PBSE_SYSTEM);
          }

        pas->as_bufsize = nsize;
        }
      else
        {
        /* str fits, clear buf */

        memset(pas->as_buf,0,pas->as_bufsize);
        }

      pas->as_next = pas->as_buf;

      /* no break, "SET" falls into "INCR" to add strings */

    case INCR:
    case MERGE:

      nsize = newpas->as_next - newpas->as_buf;   /* space needed */
      used = pas->as_next - pas->as_buf;

      if (nsize > (pas->as_bufsize - used))
        {
        need = pas->as_bufsize + 2 * nsize;  /* alloc new buf */

        if (pas->as_buf)
          pc = realloc(pas->as_buf, (size_t)need);
        else
          pc = malloc((size_t)need);

        if (pc == NULL)
          {
          return(PBSE_SYSTEM);
          }

        offset          = pc - pas->as_buf;

        pas->as_buf     = pc;
        pas->as_next    = pc + used;
        pas->as_bufsize = need;

        for (j = 0;j < pas->as_usedptr;j++) /* adjust points */
          pas->as_string[j] += offset;
        }  /* END if (nsize > (pas->as_bufsize - used)) */

      j = pas->as_usedptr + newpas->as_usedptr;

      if (j > pas->as_npointers)
        {
        struct array_strings *tmpPas;

        /* need more pointers */

        j = 3 * j / 2;  /* allocate extra     */

        need = (int)sizeof(struct array_strings) + (j - 1) * sizeof(char *);

        tmpPas = (struct array_strings *)realloc((char *)pas,(size_t)need);

        if (tmpPas == NULL)
          {
          return(PBSE_SYSTEM);
          }

        tmpPas->as_npointers = j;

        pas = tmpPas;

        attr->at_val.at_arst = pas;
        }  /* END if (j > pas->as_npointers) */

      /* now append new strings */

      for (i = 0;i < newpas->as_usedptr;i++)
        {
        char *tail;
        int   len;
        int   MatchFound; /* boolean */

        if ((op == MERGE) && (tail = strchr(newpas->as_string[i],'=')))
          {
          len = tail - newpas->as_string[i];
          MatchFound = 0;

          for (j = 0;j < pas->as_usedptr;j++)
            {
            if (!strncmp(pas->as_string[j],newpas->as_string[i],len))
              {
              MatchFound = 1;

              break;
              }
            }

          if (MatchFound == 1)
            continue;
          }

        strcpy(pas->as_next,newpas->as_string[i]);

        pas->as_string[pas->as_usedptr++] = pas->as_next;
        pas->as_next += strlen(pas->as_next) + 1;
        }  /* END for (i) */

      break;

    case DECR:

      /* decrement (remove) string from array */

      for (j = 0;j < newpas->as_usedptr;j++)
        {
        for (i = 0;i < pas->as_usedptr;i++)
          {
          if (!strcmp(pas->as_string[i], newpas->as_string[j]))
            {
            /* compact buffer */

            nsize = strlen(pas->as_string[i]) + 1;

            pc = pas->as_string[i] + nsize;

            need = pas->as_next - pc;

            memcpy(pas->as_string[i], pc, (size_t)need);

            pas->as_next -= nsize;

            /* compact pointers */

            for (++i;i < pas->as_npointers;i++)
              pas->as_string[i - 1] = pas->as_string[i] - nsize;

            pas->as_string[i - 1] = NULL;

            pas->as_usedptr--;

            break;
            }
          }
        }

      break;

    default:

      return(PBSE_INTERNAL);

      /*NOTREACHED*/

      break;
    }  /* END switch(op) */

  attr->at_flags |= ATR_VFLAG_SET | ATR_VFLAG_MODIFY;

  return(0);
  }  /* END set_arst() */





/*
 * comp_arst - compare two attributes of type ATR_TYPE_ARST
 *
 * Returns: 0 if the set of strings in the two attributes match
 *  +1 otherwise
 */

int comp_arst(

  struct attribute *attr,
  struct attribute *with)

  {
  int i;
  int j;

  struct array_strings *apa;

  struct array_strings *bpb;

  if (!attr || !with || !attr->at_val.at_arst || !with->at_val.at_arst)
    {
    return(1);
    }

  if ((attr->at_type != ATR_TYPE_ARST) ||
      (with->at_type != ATR_TYPE_ARST))
    {
    return(1);
    }

  apa = attr->at_val.at_arst;

  bpb = with->at_val.at_arst;

  for (j = 0;j < bpb->as_usedptr;j++)
    {
    for (i = 0;i < apa->as_usedptr;i++)
      {
      if (strcmp(bpb->as_string[j], apa->as_string[i]))
        {
        return(1);
        }
      }
    }

  return(0); /* all match */
  }




void free_arst(

  struct attribute *attr)

  {
  if ((attr->at_flags & ATR_VFLAG_SET) && (attr->at_val.at_arst))
    {
    if (attr->at_val.at_arst->as_buf)
      (void)free(attr->at_val.at_arst->as_buf);

    (void)free((char *)attr->at_val.at_arst);
    }

  attr->at_val.at_arst = (struct array_strings *)0;

  attr->at_flags &= ~ATR_VFLAG_SET;

  return;
  }  /* END free_arst() */





/*
 * arst_string - see if a string occurs as a prefix in an arst attribute entry
 * Search each entry in the value of an arst attribute for a sub-string
 * that begins with the passed string
 *
 * Return: pointer to string if so, NULL if not
 */

char *arst_string(

  char      *str,
  attribute *pattr)

  {
  int    i;
  size_t len;

  struct array_strings *parst;

  if ((str == NULL) || (pattr == NULL))
    {
    return(NULL);
    }

  if ((pattr->at_type != ATR_TYPE_ARST) || !(pattr->at_flags & ATR_VFLAG_SET))
    {
    /* bad type or value not set */

    return(NULL);
    }

  len = strlen(str);

  parst = pattr->at_val.at_arst;

  for (i = 0;i < parst->as_usedptr;i++)
    {
    if (!strncmp(str, parst->as_string[i], len))
      {
      return(parst->as_string[i]);
      }
    }  /* END for (i) */

  return(NULL);
  }  /* END arst_string() */

