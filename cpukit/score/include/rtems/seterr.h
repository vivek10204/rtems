/**
 *  @file  rtems/seterr.h
 *
 *  This file contains macros and definitions which ease the burden
 *  of consistently setting errno and returning -1.
 */

/*
 *  COPYRIGHT (c) 1989-2004.
 *  On-Line Applications Research Corporation (OAR).
 *
 *  The license and distribution terms for this file may be
 *  found in the file LICENSE in this distribution or at
 *  http://www.rtems.com/license/LICENSE.
 *
 *  $Id$
 */

#ifndef _RTEMS_SETERR_H
#define _RTEMS_SETERR_H

#define rtems_set_errno_and_return_minus_one( _error ) \
  do { errno = (_error); return -1; } while(0)

#define rtems_set_errno_and_return_minus_one_cast( _error, _cast ) \
  do { errno = (_error); return (_cast) -1; } while(0)

#endif
/* end of include file */
