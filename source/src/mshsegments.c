/** mshsegments.c
 * Defines shared segment creation and tracking functions.
 *
 * Copyright © 2018 Gene Harvey
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include "mshsegments.h"
#include "mshvariables.h"
#include "mlerrorutils.h"
#include "mshutils.h"
#include "mshexterntypes.h"
#include "mshlockfree.h"


#ifdef MSH_UNIX
#  include <unistd.h>
#  include <fcntl.h>
#  include <sys/mman.h>
#endif


/**
 * Does the actual segment creation operation. Write information on the segment
 * to seg_info_cache to be used immediately after.
 *
 * @note Modifies the shared memory linked list.
 * @param seg_sz The size of the segment to be opened.
 */
static void msh_CreateSegmentWorker(SegmentInfo_t* seg_info_cache, size_t data_size, int is_persistent);


/**
 * Does the actual opening operation. Writes information on the segment to seg_info_cache.
 * to be used immediately after.
 *
 * @note Interacts with but does not modify the shared memory linked list.
 * @param seg_num The segment number of the segment to be opened.
 */
static void msh_OpenSegmentWorker(SegmentInfo_t* seg_info_cache, msh_segmentnumber_t seg_num);


/**
 * Increments the revision number; avoids setting it as MSH_INITIAL_STATE.
 */
static void msh_IncrementRevisionNumber(void);


/** public function definitions **/

SharedVariableHeader_t* msh_GetSegmentData(SegmentNode_t* seg_node)
{
	/* The raw pointer is only mapped if it is actually needed.
	 * This improves performance of functions only needing the
	 * metadata without effecting performance of other functions. */
	if(msh_GetSegmentInfo(seg_node)->raw_ptr == NULL)
	{
		msh_GetSegmentInfo(seg_node)->raw_ptr = msh_MapMemory(msh_GetSegmentInfo(seg_node)->handle, msh_GetSegmentInfo(seg_node)->total_segment_size);
	}
	return (SharedVariableHeader_t*)((byte_t*)msh_GetSegmentInfo(seg_node)->raw_ptr + PadToAlignData(sizeof(SegmentMetadata_t)));
}


size_t msh_FindSegmentSize(size_t data_size)
{
	return PadToAlignData(sizeof(SegmentMetadata_t)) + data_size;
}


SegmentNode_t* msh_CreateSegment(size_t data_size, int is_persistent)
{
	SegmentInfo_t seg_info_cache;
	msh_InitializeSegmentInfo(&seg_info_cache);
	msh_CreateSegmentWorker(&seg_info_cache, data_size, is_persistent);
	return msh_CreateSegmentNode(&seg_info_cache);
}


SegmentNode_t* msh_OpenSegment(msh_segmentnumber_t seg_num)
{
	SegmentInfo_t seg_info_cache;
	msh_InitializeSegmentInfo(&seg_info_cache);
	msh_OpenSegmentWorker(&seg_info_cache, seg_num);
	return msh_CreateSegmentNode(&seg_info_cache);
}


void msh_DetachSegment(SegmentNode_t* seg_node)
{

#ifdef MSH_UNIX
	char_t segment_name[MSH_MAX_NAME_LEN];
#endif
	
	LockFreeCounter_t old_counter, new_counter;
	
	/* cache the segment info */
	SegmentInfo_t* seg_info = msh_GetSegmentInfo(seg_node);
	
	if(msh_GetVariableNode(seg_node) != NULL)
	{
		msh_RemoveVariableFromList(msh_GetVariableNode(seg_node));
		if(msh_DestroyVariable(msh_GetVariableNode(seg_node)))
		{
			return;
		}
	}
	
	if(seg_info->metadata != NULL)
	{
		/* lockfree */
		do
		{
			old_counter.span = seg_info->metadata->procs_tracking.span;
			new_counter.span = old_counter.span;
			new_counter.values.count -= 1;
			
			/* this will only lock once for each segment */
			if(old_counter.values.flag == FALSE && new_counter.values.count == 0)
			{
				msh_RemoveSegmentFromSharedList(seg_node);
				new_counter.values.flag = TRUE;
			}
		} while(msh_AtomicCompareSwap(&seg_info->metadata->procs_tracking.span, old_counter.span, new_counter.span) != old_counter.span);
		
		if(old_counter.values.flag != new_counter.values.flag)
		{
#ifdef MSH_UNIX
			msh_WriteSegmentName(segment_name, seg_info->seg_num);
			if(shm_unlink(segment_name) != 0)
			{
				meu_PrintMexError(MEU_FL, MEU_SEVERITY_SYSTEM | MEU_SEVERITY_FATAL, "UnlinkError", "There was an error unlinking the segment");
			}
#endif
			msh_SetCounterPost(&seg_info->metadata->procs_tracking, TRUE);
			msh_AtomicSubtractSize(&g_shared_info->total_shared_size, seg_info->total_segment_size);
		}
		
		msh_UnmapMemory(seg_info->metadata, sizeof(SegmentMetadata_t));
		seg_info->metadata = NULL;
		
	}
	
	if(seg_info->raw_ptr != NULL)
	{
		msh_UnmapMemory(seg_info->raw_ptr, seg_info->total_segment_size);
		seg_info->raw_ptr = NULL;
	}
	
	if(seg_info->handle != MSH_INVALID_HANDLE)
	{
		msh_CloseSharedMemory(seg_info->handle);
		seg_info->handle = MSH_INVALID_HANDLE;
	}
	
	msh_DestroySegmentNode(seg_node);
	
}


void msh_AddSegmentToSharedList(SegmentNode_t* seg_node)
{
	SegmentNode_t* last_seg_node;
	SegmentList_t* segment_cache_list = msh_GetSegmentList(seg_node) != NULL? msh_GetSegmentList(seg_node) : &g_local_seg_list;
	SegmentMetadata_t* segment_metadata = msh_GetSegmentMetadata(seg_node);
	
	msh_AcquireProcessLock(g_process_lock);
	
	/* update number of segments tracked in shared memory */
	if(g_shared_info->num_shared_segments >= g_shared_info->user_defined.max_shared_segments)
	{
		/* too many segments, unload */
		msh_RemoveSegmentFromList(seg_node);
		msh_DetachSegment(seg_node);
		meu_PrintMexError(MEU_FL,
		                  MEU_SEVERITY_USER,
		                  "TooManyVariablesError",
		                  "The shared variable would exceed the current maximum number of shared variables (currently set as %li). "
		                  "You may change this limit by using mshconfig. For more information refer to `help mshconfig`.",
		                  g_shared_info->user_defined.max_shared_segments);
	}
	else
	{
		g_shared_info->num_shared_segments += 1;
	}
	
	/* check whether to set this as the first segment number */
	if(g_shared_info->first_seg_num == MSH_INVALID_SEG_NUM)
	{
		g_shared_info->first_seg_num = msh_GetSegmentInfo(seg_node)->seg_num;
	}
	else
	{
		if((last_seg_node = msh_FindSegmentNode(&segment_cache_list->seg_table, g_shared_info->last_seg_num)) == NULL)
		{
			/* track the new segment since the mxMalloc should be cheaper than upmapping and closing the handle */
			last_seg_node = msh_OpenSegment(g_shared_info->last_seg_num);
			msh_AddSegmentToList(segment_cache_list, last_seg_node);
		}
		msh_GetSegmentMetadata(last_seg_node)->next_seg_num = msh_GetSegmentInfo(seg_node)->seg_num;
	}
	
	/* set reference to previous back of list */
	segment_metadata->prev_seg_num = g_shared_info->last_seg_num;
	
	/* set this segment as valid */
	segment_metadata->is_invalid = FALSE;
	
	/* update the last segment number */
	g_shared_info->last_seg_num = msh_GetSegmentInfo(seg_node)->seg_num;
	
	/* sign the update with this process */
	g_shared_info->update_pid = g_local_info.this_pid;
	
	/* update the revision number to indicate other processes to retrieve new segments */
	msh_IncrementRevisionNumber();
	
	msh_ReleaseProcessLock(g_process_lock);
	
}


void msh_RemoveSegmentFromSharedList(SegmentNode_t* seg_node)
{
	
	SegmentNode_t* prev_seg_node, * next_seg_node;
	SegmentList_t* segment_cache_list = msh_GetSegmentList(seg_node) != NULL? msh_GetSegmentList(seg_node) : &g_local_seg_list;
	SegmentMetadata_t* segment_metadata = msh_GetSegmentMetadata(seg_node);
	
	if(segment_metadata->is_invalid)
	{
		return;
	}
	
	msh_AcquireProcessLock(g_process_lock);
	
	/* double check volatile flag */
	if(segment_metadata->is_invalid)
	{
		msh_ReleaseProcessLock(g_process_lock);
		return;
	}
	
	/* signal that this segment is to be freed by all processes */
	segment_metadata->is_invalid = TRUE;
	
	if(msh_GetSegmentInfo(seg_node)->seg_num == g_shared_info->first_seg_num)
	{
		g_shared_info->first_seg_num = segment_metadata->next_seg_num;
	}
	else
	{
		if((prev_seg_node = msh_FindSegmentNode(&segment_cache_list->seg_table, segment_metadata->prev_seg_num)) == NULL)
		{
			/* track the new segment since the mxMalloc should be cheaper than upmapping and closing the handle */
			prev_seg_node = msh_OpenSegment(segment_metadata->prev_seg_num);
			msh_AddSegmentToList(segment_cache_list, prev_seg_node);
		}
		msh_GetSegmentMetadata(prev_seg_node)->next_seg_num = segment_metadata->next_seg_num;
	}
	
	if(msh_GetSegmentInfo(seg_node)->seg_num == g_shared_info->last_seg_num)
	{
		g_shared_info->last_seg_num = segment_metadata->prev_seg_num;
	}
	else
	{
		if((next_seg_node = msh_FindSegmentNode(&segment_cache_list->seg_table, segment_metadata->next_seg_num)) == NULL)
		{
			/* track the new segment since the mxMalloc should be cheaper than upmapping and closing the handle */
			next_seg_node = msh_OpenSegment(segment_metadata->next_seg_num);
			msh_AddSegmentToList(segment_cache_list, next_seg_node);
		}
		msh_GetSegmentMetadata(next_seg_node)->prev_seg_num = segment_metadata->prev_seg_num;
	}
	
	/* reset these to reduce confusion when debugging */
	segment_metadata->next_seg_num = MSH_INVALID_SEG_NUM;
	segment_metadata->prev_seg_num = MSH_INVALID_SEG_NUM;
	
	/* sign the update with this process */
	g_shared_info->update_pid = g_local_info.this_pid;
	
	/* update number of vars in shared memory */
	g_shared_info->num_shared_segments -= 1;
	
	/* update the revision number to tell processes to update their segment lists */
	msh_IncrementRevisionNumber();
	
	msh_ReleaseProcessLock(g_process_lock);
	
}


void msh_DetachSegmentList(SegmentList_t* seg_list)
{
	SegmentNode_t* curr_seg_node;
	while(seg_list->first != NULL)
	{
		curr_seg_node = seg_list->first;
		msh_RemoveSegmentFromList(curr_seg_node);
		msh_DetachSegment(curr_seg_node);
	}
}


void msh_ClearSharedSegments(SegmentList_t* seg_cache_list)
{
	SegmentNode_t* curr_seg_node;
	msh_AcquireProcessLock(g_process_lock);
	while(g_shared_info->first_seg_num != MSH_INVALID_SEG_NUM)
	{
		if((curr_seg_node = msh_FindSegmentNode(&seg_cache_list->seg_table, g_shared_info->first_seg_num)) == NULL)
		{
			curr_seg_node = msh_OpenSegment(g_shared_info->first_seg_num);
			msh_AddSegmentToList(seg_cache_list, curr_seg_node);
		}
		msh_RemoveSegmentFromSharedList(curr_seg_node);
		msh_RemoveSegmentFromList(curr_seg_node);
		msh_DetachSegment(curr_seg_node);
	}
	msh_ReleaseProcessLock(g_process_lock);
}


void msh_UpdateSegmentTracking(SegmentList_t* seg_list)
{
	
	msh_segmentnumber_t curr_seg_num;
	SegmentNode_t* curr_seg_node, * next_seg_node, * new_seg_node, * new_front = NULL;
	
	if(g_local_info.rev_num == g_shared_info->rev_num)
	{
		/* don't do anything if this is true */
		return;
	}
	
	msh_AcquireProcessLock(g_process_lock);
	
	for(curr_seg_num = g_shared_info->first_seg_num; curr_seg_num != MSH_INVALID_SEG_NUM; curr_seg_num = msh_GetSegmentMetadata(new_seg_node)->next_seg_num)
	{
		if((new_seg_node = msh_FindSegmentNode(&seg_list->seg_table, curr_seg_num)) == NULL)
		{
			new_seg_node = msh_OpenSegment(curr_seg_num);
			msh_AddSegmentToList(seg_list, new_seg_node);
		}
		else
		{
			msh_PlaceSegmentAtEnd(new_seg_node);
		}
		
		/* mark this as the new front if it's the first */
		if(new_front == NULL)
		{
			new_front = new_seg_node;
		}
	}
	
	/* set this process as up to date */
	g_local_info.rev_num = g_shared_info->rev_num;
	
	msh_ReleaseProcessLock(g_process_lock);
	
	/* detach the nodes that aren't in newly linked */
	for(curr_seg_node = seg_list->first; curr_seg_node != new_front; curr_seg_node = next_seg_node)
	{
		next_seg_node = msh_GetNextSegment(curr_seg_node);
		msh_RemoveSegmentFromList(curr_seg_node);
		msh_DetachSegment(curr_seg_node);
	}
	
}


void msh_UpdateLatestSegment(SegmentList_t* seg_list)
{
	SegmentNode_t* last_seg_node;
	if(g_local_info.rev_num == g_shared_info->rev_num)
	{
		return;
	}
	
	if((last_seg_node = msh_FindSegmentNode(&seg_list->seg_table, g_shared_info->last_seg_num)) != NULL)
	{
		/* place the segment at the end of the list */
		msh_PlaceSegmentAtEnd(last_seg_node);
	}
	else if(g_shared_info->last_seg_num != MSH_INVALID_SEG_NUM)
	{
		msh_AcquireProcessLock(g_process_lock);
		
		/* double check for local segment */
		if((last_seg_node = msh_FindSegmentNode(&seg_list->seg_table, g_shared_info->last_seg_num)) != NULL)
		{
			msh_ReleaseProcessLock(g_process_lock);
			/* place the segment at the end of the list */
			msh_PlaceSegmentAtEnd(last_seg_node);
		}
		else if(g_shared_info->last_seg_num != MSH_INVALID_SEG_NUM)
		{
			/* only open if the segment is valid */
			last_seg_node = msh_OpenSegment(g_shared_info->last_seg_num);
			msh_ReleaseProcessLock(g_process_lock);
			
			msh_AddSegmentToList(seg_list, last_seg_node);
		}
		else
		{
			msh_ReleaseProcessLock(g_process_lock);
		}
	}
	
}


void msh_AddSegmentToList(SegmentList_t* seg_list, SegmentNode_t* seg_node)
{
	
	/* this will be appended to the end so make sure next points to nothing */
	msh_SetNextSegment(seg_node, NULL);
	
	/* always points to the end */
	msh_SetPreviousSegment(seg_node, seg_list->last);
	
	/* set new refs */
	if(seg_list->num_segs == 0)
	{
		seg_list->first = seg_node;
	}
	else
	{
		/* set list pointer */
		msh_SetNextSegment(seg_list->last, seg_node);
	}
	
	/* place this variable at the last of the list */
	seg_list->last = seg_node;
	
	/* increment number of segments */
	seg_list->num_segs += 1;
	
	/* set the parent segment list */
	msh_SetSegmentList(seg_node, seg_list);
	
	msh_AddSegmentToTable(&seg_list->seg_table, seg_node);
	
}


void msh_PlaceSegmentAtEnd(SegmentNode_t* seg_node)
{
	/* if it's already at the end do nothing */
	if(msh_GetSegmentList(seg_node)->last == seg_node)
	{
		return;
	}
	
	/* reset local pointers */
	if(msh_GetPreviousSegment(seg_node) != NULL)
	{
		msh_SetNextSegment(msh_GetPreviousSegment(seg_node), msh_GetNextSegment(seg_node));
	}
	
	if(msh_GetNextSegment(seg_node) != NULL)
	{
		msh_SetPreviousSegment(msh_GetNextSegment(seg_node), msh_GetPreviousSegment(seg_node));
	}
	
	/* fix pointers to the front and back */
	if(msh_GetSegmentList(seg_node)->first == seg_node)
	{
		msh_GetSegmentList(seg_node)->first = msh_GetNextSegment(seg_node);
	}
	
	/* set up links at end of the list */
	msh_SetNextSegment(msh_GetSegmentList(seg_node)->last, seg_node);
	msh_SetPreviousSegment(seg_node, msh_GetSegmentList(seg_node)->last);
	
	/* reset the next pointer and place at the end of the list */
	msh_SetNextSegment(seg_node, NULL);
	msh_GetSegmentList(seg_node)->last = seg_node;
	
}


void msh_RemoveSegmentFromList(SegmentNode_t* seg_node)
{
	
	if(msh_GetSegmentList(seg_node) == NULL)
	{
		return;
	}
	
	msh_RemoveSegmentFromTable(&msh_GetSegmentList(seg_node)->seg_table, seg_node);
	
	/* reset local pointers */
	if(msh_GetPreviousSegment(seg_node) != NULL)
	{
		msh_SetNextSegment(msh_GetPreviousSegment(seg_node), msh_GetNextSegment(seg_node));
	}
	
	if(msh_GetNextSegment(seg_node) != NULL)
	{
		msh_SetPreviousSegment(msh_GetNextSegment(seg_node), msh_GetPreviousSegment(seg_node));
	}
	
	/* fix pointers to the front and back */
	if(msh_GetSegmentList(seg_node)->first == seg_node)
	{
		msh_GetSegmentList(seg_node)->first = msh_GetNextSegment(seg_node);
	}
	
	if(msh_GetSegmentList(seg_node)->last == seg_node)
	{
		msh_GetSegmentList(seg_node)->last = msh_GetPreviousSegment(seg_node);
	}
	
	msh_GetSegmentList(seg_node)->num_segs -= 1;
	
	msh_SetNextSegment(seg_node, NULL);
	msh_SetPreviousSegment(seg_node, NULL);
	msh_SetSegmentList(seg_node, NULL);
	
}


void msh_CleanSegmentList(SegmentList_t* seg_list, int shared_gc_override)
{
	SegmentNode_t* curr_seg_node, * next_seg_node;
	VariableNode_t* curr_var_node;
	
	for(curr_seg_node = seg_list->first; curr_seg_node != NULL; curr_seg_node = next_seg_node)
	{
		
		next_seg_node = msh_GetNextSegment(curr_seg_node);
		if(msh_GetSegmentMetadata(curr_seg_node)->is_invalid)
		{
			msh_RemoveSegmentFromList(curr_seg_node);
			msh_DetachSegment(curr_seg_node);
		}
		else if((curr_var_node = msh_GetVariableNode(curr_seg_node)) != NULL && met_GetCrosslink(msh_GetVariableData(curr_var_node)) == NULL)
		{
			/* does not remove the persistent variable, but does a lazy decrement of procs_using */
			if(msh_GetIsUsed(curr_var_node))
			{
				msh_SetIsUsed(curr_var_node, FALSE);
				/* decrement number of processes using this variable; if this is the last variable then GC */
				if(msh_AtomicDecrement(&msh_GetSegmentMetadata(curr_seg_node)->procs_using) == 0 && (g_shared_info->user_defined.will_shared_gc || shared_gc_override) &&
				   !msh_GetSegmentMetadata(curr_seg_node)->is_persistent)
				{
					msh_RemoveSegmentFromSharedList(curr_seg_node);
					msh_RemoveSegmentFromList(curr_seg_node);
					msh_DetachSegment(curr_seg_node);
				}
			}
		}
	}
	
}


/** static function definitions **/


static void msh_CreateSegmentWorker(SegmentInfo_t* seg_info_cache, size_t data_size, int is_persistent)
{
	char_t segment_name[MSH_MAX_NAME_LEN];
	
	/* set the segment size */
	seg_info_cache->total_segment_size = msh_FindSegmentSize(data_size);
	
	/* create a unique new segment */
	seg_info_cache->seg_num = g_shared_info->last_seg_num;
	
	if(!msh_AtomicAddSizeWithMax(&g_shared_info->total_shared_size, seg_info_cache->total_segment_size, g_shared_info->user_defined.max_shared_size))
	{
		meu_PrintMexError(MEU_FL,
		                  MEU_SEVERITY_USER,
		                  "SegmentSizeError",
		                  "The total size of currently shared memory is " SIZE_FORMAT " bytes. "
		                  "The variable shared has a size of " SIZE_FORMAT " bytes and will exceed the "
		                  "total shared size limit of " SIZE_FORMAT " bytes. You may change this limit by using mshconfig. "
		                  "For more information refer to `help mshconfig`.",
		                  g_shared_info->total_shared_size,
		                  seg_info_cache->total_segment_size,
		                  g_shared_info->user_defined.max_shared_size);
	}
	
	/* the targeted segment number is not guaranteed to be available, so keep retrying */
	do
	{
		/* change the file name */
		seg_info_cache->seg_num = (seg_info_cache->seg_num == MSH_SEG_NUM_MAX)? 0 : seg_info_cache->seg_num + 1;
		msh_WriteSegmentName(segment_name, seg_info_cache->seg_num);
	} while((seg_info_cache->handle = msh_CreateSharedMemory(segment_name, seg_info_cache->total_segment_size)) == MSH_INVALID_HANDLE);
	
	
	/* map the metadata */
	seg_info_cache->metadata = msh_MapMemory(seg_info_cache->handle, sizeof(SegmentMetadata_t));
	
	/* set the size of the segment */
	seg_info_cache->metadata->data_size = data_size;
	
	/* set the persistence flag */
	seg_info_cache->metadata->is_persistent = is_persistent;
	
	/* number of processes with variables instantiated using this segment */
	seg_info_cache->metadata->procs_using = 0;
	
	/* number of processes with a handle on this segment */
#ifdef MSH_WIN
	msh_IncrementCounter(&seg_info_cache->metadata->procs_tracking);
#else
	msh_IncrementCounter(&seg_info_cache->metadata->procs_tracking);
#endif
	
	/* set this to FALSE when it gets added to the shared list */
	seg_info_cache->metadata->is_invalid = TRUE;
	
	/* refer to nothing since this is the end of the list */
	seg_info_cache->metadata->next_seg_num = MSH_INVALID_SEG_NUM;
	
}


static void msh_OpenSegmentWorker(SegmentInfo_t* seg_info_cache, msh_segmentnumber_t seg_num)
{
	
	char_t segment_name[MSH_MAX_NAME_LEN];
	
	/* set the segment number */
	seg_info_cache->seg_num = seg_num;
	
	/* write the segment name */
	msh_WriteSegmentName(segment_name, seg_info_cache->seg_num);
	
	/* open the handle */
	seg_info_cache->handle = msh_OpenSharedMemory(segment_name);
	
	/* map the metadata */
	seg_info_cache->metadata = msh_MapMemory(seg_info_cache->handle, sizeof(SegmentMetadata_t));
	
	/* tell everyone else that another process is tracking this */
#ifdef MSH_WIN
	msh_IncrementCounter(&seg_info_cache->metadata->procs_tracking);
#else
	msh_IncrementCounter(&seg_info_cache->metadata->procs_tracking);
	
	/* check if the segment is being unlinked */
	if(msh_GetCounterFlag(&seg_info_cache->metadata->procs_tracking))
	{
		
		while(!msh_GetCounterPost(&seg_info_cache->metadata->procs_tracking));
		
		msh_UnmapMemory(seg_info_cache->metadata, sizeof(SegmentMetadata_t));
		seg_info_cache->metadata = NULL;
		
		msh_CloseSharedMemory(seg_info_cache->handle);
		seg_info_cache->handle = MSH_INVALID_HANDLE;
		
		msh_OpenSegmentWorker(seg_info_cache, seg_num);
	}
#endif
	
	/* get the segment size */
	seg_info_cache->total_segment_size = msh_FindSegmentSize(seg_info_cache->metadata->data_size);
	
}


handle_t msh_CreateSharedMemory(char_t* segment_name, size_t segment_size)
{
	
	handle_t ret_handle;

#ifdef MSH_WIN

#if MSH_BITNESS == 64
	/* split the 64-bit size */
	DWORD lo_sz = (DWORD)(segment_size & 0xFFFFFFFFL);
	DWORD hi_sz = (DWORD)((segment_size >> 32u) & 0xFFFFFFFFL);
#elif MSH_BITNESS == 32
	/* size of a memory segment can't be larger than 32 bits */
	DWORD lo_sz = (DWORD)segment_size;
	DWORD hi_sz = (DWORD)0;
#endif
	
	SetLastError(0);
	ret_handle = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, hi_sz, lo_sz, segment_name);
	if(ret_handle == NULL)
	{
		meu_PrintMexError(MEU_FL, MEU_SEVERITY_SYSTEM, "CreateFileError", "Error creating the file mapping.");
	}
	else if(GetLastError() == ERROR_ALREADY_EXISTS)
	{
		if(CloseHandle(ret_handle) == 0)
		{
			meu_PrintMexError(MEU_FL, MEU_SEVERITY_SYSTEM, "CloseHandleError", "Error closing the file handle.");
		}
		return MSH_INVALID_HANDLE;
	}

#else
	
	/* errno is not set unless the function fails, so reset it each time */
	errno = 0;
	ret_handle = shm_open(segment_name, O_RDWR | O_CREAT | O_EXCL, g_shared_info->user_defined.security);
	if(ret_handle == -1)
	{
		if(errno != EEXIST)
		{
			meu_PrintMexError(MEU_FL, MEU_SEVERITY_SYSTEM, "CreateError", "There was an error creating the segment");
		}
		return MSH_INVALID_HANDLE;
	}
	
	/* set the segment size */
	if(ftruncate(ret_handle, segment_size) != 0)
	{
		meu_PrintMexError(MEU_FL, MEU_SEVERITY_SYSTEM, "TruncateError", "There was an error truncating the segment");
	}

#endif
	
	return ret_handle;
	
}


handle_t msh_OpenSharedMemory(char_t* segment_name)
{
	
	handle_t ret_handle;

#ifdef MSH_WIN
	/* get the new file handle */
	ret_handle = OpenFileMapping(FILE_MAP_ALL_ACCESS, TRUE, segment_name);
	if(ret_handle == NULL)
	{
		if(GetLastError() == ERROR_FILE_NOT_FOUND)
		{
			meu_PrintMexError(MEU_FL, MEU_SEVERITY_SYSTEM | MEU_SEVERITY_FATAL, "VariableNotFoundError", "There was an error where matshare lost track of a variable. "
			                                                                                                             "This is usually because a linked session of MATLAB was "
			                                                                                                             "terminated unexpectedly, which can occur when closing a parpool. "
			                                                                                                             "To prevent this, make a call to mshdetach before closing the parpool.");
		}
		meu_PrintMexError(MEU_FL, MEU_SEVERITY_SYSTEM, "OpenFileError", "Error opening the file mapping.");
	}
#else
	ret_handle = shm_open(segment_name, O_RDWR, g_shared_info->user_defined.security);
	if(ret_handle == -1)
	{
		meu_PrintMexError(MEU_FL, MEU_SEVERITY_SYSTEM, "OpenError", "There was an error opening the segment");
	}
#endif
	
	return ret_handle;
	
}


void* msh_MapMemory(handle_t segment_handle, size_t map_sz)
{
	
	void* seg_ptr;

#ifdef MSH_WIN
	seg_ptr = MapViewOfFile(segment_handle, FILE_MAP_ALL_ACCESS, 0, 0, map_sz);
	if(seg_ptr == NULL)
	{
		meu_PrintMexError(MEU_FL, MEU_SEVERITY_SYSTEM, "MemoryMappingError", "There was an error memory mapping the segment");
	}
#else
	seg_ptr = mmap(NULL, map_sz, PROT_READ | PROT_WRITE, MAP_SHARED, segment_handle, 0);
	if(seg_ptr == MAP_FAILED)
	{
		meu_PrintMexError(MEU_FL, MEU_SEVERITY_SYSTEM, "MemoryMappingError", "There was an error memory mapping the segment");
	}
#endif
	
	return seg_ptr;
	
}


void msh_UnmapMemory(void* segment_pointer, size_t map_sz)
{
#ifdef MSH_WIN
	if(UnmapViewOfFile(segment_pointer) == 0)
	{
		meu_PrintMexError(MEU_FL, MEU_SEVERITY_SYSTEM, "UnmapFileError", "Error unmapping the file.");
	}
#else
	if(munmap(segment_pointer, map_sz) != 0)
	{
		meu_PrintMexError(MEU_FL, MEU_SEVERITY_SYSTEM, "MunmapError", "There was an error unmapping the segment");
	}
#endif
}


void msh_CloseSharedMemory(handle_t segment_handle)
{
#ifdef MSH_WIN
	if(CloseHandle(segment_handle) == 0)
	{
		meu_PrintMexError(MEU_FL, MEU_SEVERITY_SYSTEM, "CloseHandleError", "Error closing the data file handle.");
	}
#else
	if(close(segment_handle) == -1)
	{
		meu_PrintMexError(MEU_FL, MEU_SEVERITY_SYSTEM, "CloseHandleError", "Error closing the data file handle.");
	}
#endif
}


void msh_LockMemory(void* ptr, size_t sz)
{
#ifdef MSH_WIN
	VirtualLock(ptr, sz);
#else
	/* unlock this set of pages */
	mlock(ptr, sz);
#endif
}


void msh_UnlockMemory(void* ptr, size_t sz)
{
#ifdef MSH_WIN
	VirtualUnlock(ptr, sz);
#else
	/* unlock this set of pages */
	munlock(ptr, sz);
#endif
}


static void msh_IncrementRevisionNumber(void)
{
	/* make sure to avoid setting this as MSH_INITIAL_STATE */
	g_shared_info->rev_num = (g_shared_info->rev_num == SIZE_MAX)? 1 : g_shared_info->rev_num + 1;
}