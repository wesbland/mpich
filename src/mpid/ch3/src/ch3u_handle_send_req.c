/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 *  (C) 2001 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

#include "mpidimpl.h"
#include "mpidrma.h"

#undef FUNCNAME
#define FUNCNAME MPIDI_CH3U_Handle_send_req
#undef FCNAME
#define FCNAME MPIDI_QUOTE(FUNCNAME)
int MPIDI_CH3U_Handle_send_req(MPIDI_VC_t * vc, MPID_Request * sreq, 
			       int *complete)
{
    int mpi_errno = MPI_SUCCESS;
    int (*reqFn)(MPIDI_VC_t *, MPID_Request *, int *);
    MPIDI_STATE_DECL(MPID_STATE_MPIDI_CH3U_HANDLE_SEND_REQ);

    MPIDI_FUNC_ENTER(MPID_STATE_MPIDI_CH3U_HANDLE_SEND_REQ);

    /* Use the associated function rather than switching on the old ca field */
    /* Routines can call the attached function directly */
    reqFn = sreq->dev.OnDataAvail;
    if (!reqFn) {
	MPIU_Assert(MPIDI_Request_get_type(sreq) != MPIDI_REQUEST_TYPE_GET_RESP);
	MPIDI_CH3U_Request_complete(sreq);
        *complete = 1;
    }
    else {
	mpi_errno = reqFn( vc, sreq, complete );
    }

    MPIDI_FUNC_EXIT(MPID_STATE_MPIDI_CH3U_HANDLE_SEND_REQ);
    return mpi_errno;
}

/* ----------------------------------------------------------------------- */
/* Here are the functions that implement the actions that are taken when 
 * data is available for a send request (or other completion operations)
 * These include "send" requests that are part of the RMA implementation.
 */
/* ----------------------------------------------------------------------- */

int MPIDI_CH3_ReqHandler_GetSendComplete( MPIDI_VC_t *vc ATTRIBUTE((unused)),
					      MPID_Request *sreq, 
					      int *complete )
{
    int mpi_errno = MPI_SUCCESS;
    MPID_Win *win_ptr;

    MPID_Win_get_ptr(sreq->dev.target_win_handle, win_ptr);

    if (sreq->dev.flags & MPIDI_CH3_PKT_FLAG_RMA_UNLOCK) {
        mpi_errno = MPIDI_CH3I_Release_lock(win_ptr);
        if (mpi_errno) MPIU_ERR_POP(mpi_errno);
        MPIDI_CH3_Progress_signal_completion();
    }
    if (sreq->dev.flags & MPIDI_CH3_PKT_FLAG_RMA_DECR_AT_COUNTER) {
        win_ptr->at_completion_counter--;
        MPIU_Assert(win_ptr->at_completion_counter >= 0);
        /* Signal the local process when the op counter reaches 0. */
        if (win_ptr->at_completion_counter == 0)
            MPIDI_CH3_Progress_signal_completion();
    }

    /* here we decrement the Active Target counter to guarantee the GET-like
       operation are completed when counter reaches zero. */
    win_ptr->at_completion_counter--;
    MPIU_Assert(win_ptr->at_completion_counter >= 0);

    /* mark data transfer as complete and decrement CC */
    MPIDI_CH3U_Request_complete(sreq);
    *complete = TRUE;

 fn_exit:
    return mpi_errno;
 fn_fail:
    goto fn_exit;
}

#undef FUNCNAME
#define FUNCNAME MPIDI_CH3_ReqHandler_GaccumLikeSendComplete
#undef FCNAME
#define FCNAME MPIDI_QUOTE(FUNCNAME)
int MPIDI_CH3_ReqHandler_GaccumLikeSendComplete( MPIDI_VC_t *vc,
                                                 MPID_Request *rreq,
                                                 int *complete )
{
    int mpi_errno = MPI_SUCCESS;
    MPID_Win *win_ptr;
    MPIDI_STATE_DECL(MPID_STATE_MPIDI_CH3_REQHANDLER_GACCUMLIKESENDCOMPLETE);

    MPIDI_FUNC_ENTER(MPID_STATE_MPIDI_CH3_REQHANDLER_GACCUMLIKESENDCOMPLETE);
    /* This function is triggered when sending back process of GACC/FOP/CAS
       is finished. Only GACC used user_buf. FOP and CAS can fit all data
       in response packet. */
    if (rreq->dev.user_buf != NULL)
        MPIU_Free(rreq->dev.user_buf);

    MPID_Win_get_ptr(rreq->dev.target_win_handle, win_ptr);

    /* here we decrement the Active Target counter to guarantee the GET-like
       operation are completed when counter reaches zero. */
    win_ptr->at_completion_counter--;
    MPIU_Assert(win_ptr->at_completion_counter >= 0);

    if (rreq->dev.flags & MPIDI_CH3_PKT_FLAG_RMA_UNLOCK) {
        mpi_errno = MPIDI_CH3I_Release_lock(win_ptr);
        if (mpi_errno) MPIU_ERR_POP(mpi_errno);
        MPIDI_CH3_Progress_signal_completion();
    }

    if (rreq->dev.flags & MPIDI_CH3_PKT_FLAG_RMA_DECR_AT_COUNTER) {
        win_ptr->at_completion_counter--;
        MPIU_Assert(win_ptr->at_completion_counter >= 0);
        /* Signal the local process when the op counter reaches 0. */
        if (win_ptr->at_completion_counter == 0)
            MPIDI_CH3_Progress_signal_completion();
    }

    MPIDI_CH3U_Request_complete(rreq);
    *complete = TRUE;
 fn_exit:
    MPIDI_FUNC_EXIT(MPID_STATE_MPIDI_CH3_REQHANDLER_GACCUMLIKESENDCOMPLETE);
    return mpi_errno;

 fn_fail:
    goto fn_exit;
}

int MPIDI_CH3_ReqHandler_SendReloadIOV( MPIDI_VC_t *vc ATTRIBUTE((unused)), MPID_Request *sreq, 
					int *complete )
{
    int mpi_errno;

    /* setting the iov_offset to 0 here is critical, since it is intentionally
     * not set in the _load_send_iov function */
    sreq->dev.iov_offset = 0;
    sreq->dev.iov_count = MPID_IOV_LIMIT;
    mpi_errno = MPIDI_CH3U_Request_load_send_iov(sreq, sreq->dev.iov, 
						 &sreq->dev.iov_count);
    if (mpi_errno != MPI_SUCCESS) {
	MPIU_ERR_SETFATALANDJUMP(mpi_errno, MPI_ERR_OTHER,"**ch3|loadsendiov");
    }
	    
    *complete = FALSE;

 fn_fail:
    return mpi_errno;
}
