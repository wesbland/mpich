/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 *  (C) 2006 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 *
 *  Portions of this code were written by Intel Corporation.
 *  Copyright (C) 2011-2017 Intel Corporation.  Intel provides this material
 *  to Argonne National Laboratory subject to Software Grant and Corporate
 *  Contributor License Agreement dated February 8, 2012.
 */
#ifndef POSIX_IMPL_H_INCLUDED
#define POSIX_IMPL_H_INCLUDED

#include <mpidimpl.h>
#include "mpidch4r.h"

#include "posix_types.h"
#include "posix_eager.h"
#include "posix_eager_impl.h"

#undef FUNCNAME
#define FUNCNAME nothing
#define BEGIN_FUNC(FUNCNAME)                    \
    MPIR_FUNC_VERBOSE_STATE_DECL(FUNCNAME);     \
    MPIR_FUNC_VERBOSE_ENTER(FUNCNAME);
#define END_FUNC(FUNCNAME)                      \
    MPIR_FUNC_VERBOSE_EXIT(FUNCNAME);
#define END_FUNC_RC(FUNCNAME)                   \
  fn_exit:                                      \
    MPIR_FUNC_VERBOSE_EXIT(FUNCNAME);           \
    return mpi_errno;                           \
  fn_fail:                                      \
    goto fn_exit;

#define __SHORT_FILE__                          \
    (strrchr(__FILE__,'/')                      \
     ? strrchr(__FILE__,'/')+1                  \
     : __FILE__                                 \
)

/*
 * Wrapper routines of process mutex for shared memory RMA.
 * Called by both POSIX RMA and fallback AM handlers through CS hooks.
 */
#define MPIDI_POSIX_RMA_MUTEX_INIT(mutex_ptr) do {                                  \
    int pt_err = MPL_PROC_MUTEX_SUCCESS;                                            \
    MPL_proc_mutex_create(mutex_ptr, &pt_err);                                      \
    MPIR_ERR_CHKANDJUMP1(pt_err != MPL_PROC_MUTEX_SUCCESS, mpi_errno,               \
                         MPI_ERR_OTHER, "**windows_mutex",                          \
                         "**windows_mutex %s", "MPL_proc_mutex_create");            \
} while (0);

#define MPIDI_POSIX_RMA_MUTEX_DESTROY(mutex_ptr)  do {                              \
    int pt_err = MPL_PROC_MUTEX_SUCCESS;                                            \
    MPL_proc_mutex_destroy(mutex_ptr, &pt_err);                                     \
    MPIR_ERR_CHKANDJUMP1(pt_err != MPL_PROC_MUTEX_SUCCESS, mpi_errno,               \
                         MPI_ERR_OTHER, "**windows_mutex",                          \
                         "**windows_mutex %s", "MPL_proc_mutex_destroy");           \
} while (0);

#define MPIDI_POSIX_RMA_MUTEX_LOCK(mutex_ptr) do {                                  \
    int pt_err = MPL_PROC_MUTEX_SUCCESS;                                            \
    MPL_proc_mutex_lock(mutex_ptr, &pt_err);                                        \
    MPIR_ERR_CHKANDJUMP1(pt_err != MPL_PROC_MUTEX_SUCCESS, mpi_errno,               \
                         MPI_ERR_OTHER, "**windows_mutex",                          \
                         "**windows_mutex %s", "MPL_proc_mutex_lock");              \
} while (0)

#define MPIDI_POSIX_RMA_MUTEX_UNLOCK(mutex_ptr) do {                                \
        int pt_err = MPL_PROC_MUTEX_SUCCESS;                                        \
        MPL_proc_mutex_unlock(mutex_ptr, &pt_err);                                  \
        MPIR_ERR_CHKANDJUMP1(pt_err != MPL_PROC_MUTEX_SUCCESS, mpi_errno,           \
                             MPI_ERR_OTHER, "**windows_mutex",                      \
                             "**windows_mutex %s", "MPL_proc_mutex_unlock");        \
} while (0)

#endif /* POSIX_IMPL_H_INCLUDED */
