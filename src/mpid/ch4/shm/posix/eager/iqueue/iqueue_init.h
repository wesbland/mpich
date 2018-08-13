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
#ifndef POSIX_EAGER_IQUEUE_INIT_H_INCLUDED
#define POSIX_EAGER_IQUEUE_INIT_H_INCLUDED

#include "iqueue_types.h"

/*
=== BEGIN_MPI_T_CVAR_INFO_BLOCK ===

cvars:
    - name        : MPIR_CVAR_CH4_SHM_POSIX_IQUEUE_NUM_CELLS
      category    : CH4
      type        : int
      default     : 32
      class       : device
      verbosity   : MPI_T_VERBOSITY_USER_BASIC
      scope       : MPI_T_SCOPE_ALL_EQ
      description : >-
        The number of cells used for the depth of the iqueue.

=== END_MPI_T_CVAR_INFO_BLOCK ===
*/

#undef FUNCNAME
#define FUNCNAME MPIDI_POSIX_eager_init
#undef FCNAME
#define FCNAME MPL_QUOTE(FUNCNAME)
MPL_STATIC_INLINE_PREFIX int MPIDI_POSIX_eager_init(int rank, int size)
{
    MPIR_CHKPMEM_DECL(2);
    int mpi_errno = MPI_SUCCESS;
    int i, num_local, local_rank_0, local_rank;
    int *local_ranks, *local_procs;
    MPIDI_POSIX_EAGER_IQUEUE_transport_t *transport;
    size_t size_of_terminals;
    size_t size_of_cells;
    size_t size_of_shared_memory;
    MPIDI_av_entry_t *av = NULL;

    /* Get the internal data structure to describe the iqueues */
    /* XXX - Why is this not just a global struct? Why is it abstracted inside this call? Do we ever
     * need more than one of these? */
    transport = MPIDI_POSIX_EAGER_IQUEUE_get_transport();

    MPIR_CHKPMEM_MALLOC(local_procs, int *, size * sizeof(int), mpi_errno,
                        "local process index array", MPL_MEM_SHM);

    MPIR_CHKPMEM_MALLOC(local_ranks, int *, size * sizeof(int), mpi_errno,
                        "mem_region local ranks", MPL_MEM_SHM);

    /* Calculate the number of local ranks and this process's rank among those local ranks. */
    num_local = 0;
    local_rank = -1;
    local_rank_0 = -1;
    for (i = 0; i < size; i++) {
        av = MPIDIU_comm_rank_to_av(MPIR_Process.comm_world, i);
        if (MPIDI_av_is_local(av)) {
            if (i == rank) {
                local_rank = num_local;
            }
            if (local_rank_0 == -1) {
                local_rank_0 = i;
            }
            local_procs[num_local] = i;
            local_ranks[i] = num_local;
            num_local++;
        }
    }

    transport->local_rank = local_rank;
    transport->num_local = num_local;
    transport->num_cells = MPIR_CVAR_CH4_SHM_POSIX_IQUEUE_NUM_CELLS;
    transport->size_of_cell = MPIDI_POSIX_EAGER_IQUEUE_DEFAULT_CELL_SIZE;

    transport->local_ranks = local_ranks;
    transport->local_procs = local_procs;

    /* Create one terminal for each process with which we will be able to communicate. */
    size_of_terminals = (size_t) transport->num_local * sizeof(MPIDI_POSIX_EAGER_IQUEUE_terminal_t);

    /* Behind each terminal is a series of cells. We have `num_cells` per queue/terminal per
     * communicating process. */
    size_of_cells = (size_t) transport->num_local * (size_t) transport->num_cells
        * (size_t) transport->size_of_cell;

    size_of_shared_memory = size_of_terminals + size_of_cells;

    /* Create the shared memory regions that will be used for the iqueue cells and terminals. */
    mpi_errno = MPIDU_shm_seg_alloc(size_of_shared_memory,
                                    &transport->pointer_to_shared_memory, MPL_MEM_SHM);
    if (mpi_errno) {
        MPIR_ERR_POP(mpi_errno);
    }

    mpi_errno = MPIDU_shm_seg_commit(&transport->memory, &transport->barrier,
                                     num_local, local_rank, local_rank_0, rank, MPL_MEM_SHM);
    if (mpi_errno) {
        MPIR_ERR_POP(mpi_errno);
    }

    /* Set up the appropriate pointers for each of the parts of the queues. */
    transport->terminals =
        (MPIDI_POSIX_EAGER_IQUEUE_terminal_t *) ((char *) transport->pointer_to_shared_memory);

    transport->cells = (char *) transport->pointer_to_shared_memory
        + size_of_terminals + (size_t) transport->local_rank * (size_t) transport->num_cells *
        (size_t) transport->size_of_cell;

    transport->terminals[transport->local_rank].head = 0;

    /* Do the pointer arithmetic and initialize each of the cell data structures. */
    for (i = 0; i < transport->num_cells; i++) {
        MPIDI_POSIX_EAGER_IQUEUE_cell_t *cell = (MPIDI_POSIX_EAGER_IQUEUE_cell_t *)
            ((char *) transport->cells + (size_t) transport->size_of_cell * i);
        cell->type = MPIDI_POSIX_EAGER_IQUEUE_CELL_TYPE_NULL;
        cell->from = transport->local_rank;
        cell->next = NULL;
        cell->prev = 0;
        cell->payload_size = 0;
    }

    /* Run local procs barrier */
    mpi_errno = MPIDU_shm_barrier(transport->barrier, num_local);
    if (mpi_errno) {
        MPIR_ERR_POP(mpi_errno);
    }

    MPIR_CHKPMEM_COMMIT();

  fn_exit:
    MPIR_FUNC_VERBOSE_EXIT(MPID_STATE_MPIDI_SHM_INIT);
    return mpi_errno;
  fn_fail:
    /* --BEGIN ERROR HANDLING-- */
    MPIR_CHKPMEM_REAP();
    goto fn_exit;
    /* --END ERROR HANDLING-- */
}

#undef FUNCNAME
#define FUNCNAME MPIDI_POSIX_eager_finalize
#undef FCNAME
#define FCNAME MPL_QUOTE(FUNCNAME)
MPL_STATIC_INLINE_PREFIX int MPIDI_POSIX_eager_finalize()
{
    MPIDI_POSIX_EAGER_IQUEUE_transport_t *transport;
    int mpi_errno;

    transport = MPIDI_POSIX_EAGER_IQUEUE_get_transport();

    /* Make sure all local processes are ready to destroy the segment together */
    mpi_errno = MPIDU_shm_barrier(transport->barrier, transport->num_local);
    if (mpi_errno) {
        MPIR_ERR_POP(mpi_errno);
    }

    MPL_free(transport->local_ranks);
    MPL_free(transport->local_procs);

    mpi_errno = MPIDU_shm_seg_destroy(&transport->memory, transport->num_local);

  fn_exit:
    MPIR_FUNC_VERBOSE_EXIT(MPID_STATE_MPIDI_SHM_INIT);
    return mpi_errno;
  fn_fail:
    goto fn_exit;
}

#endif /* POSIX_EAGER_IQUEUE_INIT_H_INCLUDED */
