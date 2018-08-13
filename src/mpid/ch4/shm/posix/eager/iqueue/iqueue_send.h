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
#ifndef POSIX_EAGER_IQUEUE_SEND_H_INCLUDED
#define POSIX_EAGER_IQUEUE_SEND_H_INCLUDED

#include "iqueue_impl.h"

MPL_STATIC_INLINE_PREFIX MPIDI_POSIX_EAGER_IQUEUE_cell_t
    * MPIDI_POSIX_EAGER_IQUEUE_new_cell(MPIDI_POSIX_EAGER_IQUEUE_transport_t * transport)
{
    int i;
    for (i = 0; i < transport->num_cells; i++) {
        MPIDI_POSIX_EAGER_IQUEUE_cell_t *cell = MPIDI_POSIX_EAGER_IQUEUE_THIS_CELL(transport, i);
        if (cell->type == MPIDI_POSIX_EAGER_IQUEUE_CELL_TYPE_NULL) {
            return cell;
        }
    }
    return NULL;
}

#undef FUNCNAME
#define FUNCNAME MPIDI_POSIX_eager_send
#undef FCNAME
#define FCNAME MPL_QUOTE(FUNCNAME)
MPL_STATIC_INLINE_PREFIX int
MPIDI_POSIX_eager_send(int grank,
                       MPIDI_POSIX_am_header_t ** msg_hdr,
                       struct iovec **iov, size_t * iov_num, int is_blocking)
{
    MPIDI_POSIX_EAGER_IQUEUE_transport_t *transport;
    MPIDI_POSIX_EAGER_IQUEUE_cell_t *cell;

    /* Get the transport object that holds all of the global variables. */
    transport = MPIDI_POSIX_EAGER_IQUEUE_get_transport();

    /* Try to get a new cell to hold the message */
    cell = MPIDI_POSIX_EAGER_IQUEUE_new_cell(transport);

    /* If a cell was available, use it to send the message. */
    if (likely(cell)) {
        MPIDI_POSIX_EAGER_IQUEUE_terminal_t *terminal;
        size_t i, iov_done, capacity, available;
        uintptr_t prev, handle;
        char *payload;

        /* Find the correct queue for this rank pair. */
        terminal = &transport->terminals[transport->local_ranks[grank]];

        /* Get the offset of the cell in the queue */
        handle = MPIDI_POSIX_EAGER_IQUEUE_GET_HANDLE(transport, cell);

        /* Get the memory allocated to be used for the message transportation. */
        payload = MPIDI_POSIX_EAGER_IQUEUE_CELL_PAYLOAD(cell);

        /* Figure out the capacity of each cell */
        capacity = MPIDI_POSIX_EAGER_IQUEUE_CELL_CAPACITY(transport);
        available = capacity;

        /* If this is the beginning of the message, mark it as the head. Otherwise it will be the
         * tail. */
        if (*msg_hdr) {
            cell->am_header = **msg_hdr;
            *msg_hdr = NULL;    /* completed header copy */
            cell->type = MPIDI_POSIX_EAGER_IQUEUE_CELL_TYPE_HEAD;
        } else {
            cell->type = MPIDI_POSIX_EAGER_IQUEUE_CELL_TYPE_TAIL;
        }

        /* Pack the data into the cells */
        iov_done = 0;
        for (i = 0; i < *iov_num; i++) {
            /* Optimize for the case where the message will fit into the cell. */
            if (likely(available >= (*iov)[i].iov_len)) {
                memcpy(payload, (*iov)[i].iov_base, (*iov)[i].iov_len);

                payload += (*iov)[i].iov_len;
                available -= (*iov)[i].iov_len;

                iov_done++;
            } else {
                /* If the message won't fit, put as much as we can and update the iovec for the next
                 * time around. */
                memcpy(payload, (*iov)[i].iov_base, available);

                (*iov)[i].iov_base = (char *) (*iov)[i].iov_base + available;
                (*iov)[i].iov_len -= available;

                available = 0;

                break;
            }
        }

        cell->payload_size = capacity - available;

        /* Move the flag to indicate the head. */
        do {
            prev = terminal->head;
            cell->prev = prev;
            OPA_compiler_barrier();
        } while (((uintptr_t) (unsigned int *)
                  /* Swaps the head of the terminal with the current handle if the previous head has
                   * now been consumed. Continues until we swap out the prev pointer. */
                  OPA_cas_ptr((OPA_ptr_t *) & terminal->head, (void *) prev, (void *) handle)
                  != prev));

        /* Update the user counter for number of iovecs left */
        *iov_num -= iov_done;

        /* Check to see if we finished all of the iovecs that came from the caller. If not, update
         * the iov pointer. If so, set it to NULL. Either way, the caller will know the status of
         * the operation from the value of iov. */
        if (*iov_num) {
            *iov = &((*iov)[iov_done]);
        } else {
            *iov = NULL;
        }

        return MPIDI_POSIX_OK;
    }
    /* If a cell wasn't available, let the caller know that we weren't able to send the message
     * immediately. */
    else {
        return MPIDI_POSIX_NOK;
    }
}

#endif /* POSIX_EAGER_IQUEUE_SEND_H_INCLUDED */
