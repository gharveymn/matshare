#include "headers/mshsegments.h"
#include "headers/mshvariables.h"
#include "headers/matlabutils.h"
#include "headers/mshutils.h"

/**
 * Does the actual segment creation operation. Write information on the segment
 * to seg_info_cache to be used immediately after.
 *
 * @note Modifies the shared memory linked list.
 * @param seg_sz The size of the segment to be opened.
 */
static void msh_CreateSegmentWorker(SegmentInfo_t* seg_info_cache);


/**
 * Does the actual opening operation. Writes information on the segment to seg_info_cache.
 * to be used immediately after.
 *
 * @note Interacts with but does not modify the shared memory linked list.
 * @param seg_num The segment number of the segment to be opened.
 */
static void msh_OpenSegmentWorker(SegmentInfo_t* seg_info_cache);


/**
 * Create a new segment node.
 *
 * @note Completely local.
 * @param seg_info A struct containing information about a shared memory segment created either by OpenSegment or CreateSegment.
 * @return The newly allocated segment node.
 */
static SegmentNode_t* msh_CreateSegmentNode(SegmentInfo_t* seg_info_cache);

static void msh_DestroySegmentNode(SegmentNode_t* seg_node);

static handle_t msh_OpenSegmentHandle(msh_segmentnumber_t seg_num);

static void* msh_MapSegment(handle_t segment_handle, size_t map_sz);

static void msh_UnmapSegment(void* segment_pointer, size_t map_sz);

static void msh_CloseSegmentHandle(handle_t segment_handle);

/*
static SegmentNode_t* msh_FindSegment(SegmentList_t* seg_list, msh_segmentnumber_t comp_seg_num);
*/


/** public function definitions **/


SegmentMetadata_t* msh_GetSegmentMetadata(SegmentNode_t* seg_node)
{
	return (SegmentMetadata_t*)seg_node->seg_info.shared_memory_ptr;
}

SharedVariableHeader_t* msh_GetSegmentData(SegmentNode_t* seg_node)
{
	return (SharedVariableHeader_t*)((byte_t*)seg_node->seg_info.shared_memory_ptr + PadToAlignData(sizeof(SegmentMetadata_t)));
}


size_t msh_FindSegmentSize(const mxArray* in_var)
{
	return msh_FindSharedSize(in_var) + PadToAlignData(sizeof(SegmentMetadata_t));
}


SegmentNode_t* msh_CreateSegment(size_t seg_sz)
{
	SegmentInfo_t seg_info_cache;
	seg_info_cache.seg_sz = seg_sz;
	msh_CreateSegmentWorker(&seg_info_cache);
	return msh_CreateSegmentNode(&seg_info_cache);
}


SegmentNode_t* msh_OpenSegment(msh_segmentnumber_t seg_num)
{
	SegmentInfo_t seg_info_cache;
	seg_info_cache.seg_num = seg_num;
	msh_OpenSegmentWorker(&seg_info_cache);
	return msh_CreateSegmentNode(&seg_info_cache);
}


void msh_DetachSegment(SegmentNode_t* seg_node)
{
	
	if(seg_node->var_node != NULL)
	{
		msh_DestroyVariable(seg_node->var_node);
	}
	
	if(seg_node->seg_info.is_mapped)
	{
		msh_AtomicDecrement(&msh_GetSegmentMetadata(seg_node)->procs_tracking);
		msh_UnmapSegment(seg_node->seg_info.shared_memory_ptr, seg_node->seg_info.seg_sz);
		seg_node->seg_info.is_mapped = FALSE;
		
	}
	
	if(seg_node->seg_info.is_init)
	{
		msh_CloseSegmentHandle(seg_node->seg_info.handle);
		seg_node->seg_info.is_init = FALSE;
	}
	
	msh_DestroySegmentNode(seg_node);
	
}


void msh_AddSegmentToSharedList(SegmentNode_t* seg_node)
{
	handle_t temp_segment_handle;
	SegmentNode_t* temp_seg_node;
	SegmentMetadata_t* temp_segment_metadata,* segment_metadata = msh_GetSegmentMetadata(seg_node);
	
	msh_AcquireProcessLock();
	
	if(g_shared_info->user_def.sharetype == msh_SHARETYPE_OVERWRITE)
	{
		msh_ClearSegmentList(&g_local_seg_list);
	}
	
	/* check whether to set this as the first segment number */
	if(g_shared_info->num_valid_segments == 0)
	{
		g_shared_info->first_seg_num = seg_node->seg_info.seg_num;
	}
	else
	{
		if((temp_seg_node = msh_FindSegmentNode(&g_local_seg_list.seg_table, g_shared_info->last_seg_num)) != NULL && !msh_GetSegmentMetadata(temp_seg_node)->is_invalid)
		{
			msh_GetSegmentMetadata(temp_seg_node)->next_seg_num = seg_node->seg_info.seg_num;
		}
		else
		{
			/* make a temporary mapping to modify the metadata */
			temp_segment_handle = msh_OpenSegmentHandle(g_shared_info->last_seg_num);
			temp_segment_metadata = msh_MapSegment(temp_segment_handle, sizeof(SegmentMetadata_t));
			
			temp_segment_metadata->next_seg_num = seg_node->seg_info.seg_num;
			
			msh_UnmapSegment(temp_segment_metadata, sizeof(SegmentMetadata_t));
			msh_CloseSegmentHandle(temp_segment_handle);
		}
	}
	
	/* set reference to previous back of list */
	segment_metadata->prev_seg_num = g_shared_info->last_seg_num;
	
	/* update the last segment number */
	g_shared_info->last_seg_num = seg_node->seg_info.seg_num;
	
	/* update number of vars in shared memory */
	g_shared_info->num_valid_segments += 1;
	
	/* sign the update with this process */
	g_shared_info->update_pid = g_local_info->this_pid;
	
	/* update the revision number to indicate other processes to retrieve new segments */
	g_shared_info->rev_num += 1;
	
	msh_ReleaseProcessLock();
	
}


void msh_RemoveSegmentFromSharedList(SegmentNode_t* seg_node)
{

#ifdef MSH_UNIX
	char_t segment_name[MSH_MAX_NAME_LEN];
#endif
	
	handle_t temp_segment_handle;
	SegmentNode_t* temp_seg_node;
	SegmentMetadata_t* temp_segment_metadata,* segment_metadata = msh_GetSegmentMetadata(seg_node);
	
	msh_AcquireProcessLock();
	
	if(segment_metadata->is_invalid == TRUE)
	{
		msh_ReleaseProcessLock();
		return;
	}
	
	/* signal that this segment is to be freed by all processes */
	segment_metadata->is_invalid = TRUE;
	
	if(seg_node->seg_info.seg_num == g_shared_info->first_seg_num)
	{
		g_shared_info->first_seg_num = segment_metadata->next_seg_num;
	}
	else
	{
		if((temp_seg_node = msh_FindSegmentNode(&g_local_seg_list.seg_table, segment_metadata->prev_seg_num)) != NULL && !msh_GetSegmentMetadata(temp_seg_node)->is_invalid)
		{
			msh_GetSegmentMetadata(temp_seg_node)->next_seg_num = segment_metadata->next_seg_num;
		}
		else
		{
			/* make a temporary mapping to modify the metadata */
			temp_segment_handle = msh_OpenSegmentHandle(segment_metadata->prev_seg_num);
			temp_segment_metadata = msh_MapSegment(temp_segment_handle, sizeof(SegmentMetadata_t));
			
			temp_segment_metadata->next_seg_num = segment_metadata->next_seg_num;
			
			msh_UnmapSegment(temp_segment_metadata, sizeof(SegmentMetadata_t));
			msh_CloseSegmentHandle(temp_segment_handle);
		}
	}
	
	if(seg_node->seg_info.seg_num == g_shared_info->last_seg_num)
	{
		g_shared_info->last_seg_num = segment_metadata->prev_seg_num;
	}
	else
	{
		if((temp_seg_node = msh_FindSegmentNode(&g_local_seg_list.seg_table, segment_metadata->next_seg_num)) != NULL && !msh_GetSegmentMetadata(temp_seg_node)->is_invalid)
		{
			msh_GetSegmentMetadata(temp_seg_node)->prev_seg_num = segment_metadata->prev_seg_num;
		}
		else
		{
			/* make a temporary mapping to modify the metadata */
			temp_segment_handle = msh_OpenSegmentHandle(segment_metadata->next_seg_num);
			temp_segment_metadata = msh_MapSegment(temp_segment_handle, sizeof(SegmentMetadata_t));
			
			temp_segment_metadata->prev_seg_num = segment_metadata->prev_seg_num;
			
			msh_UnmapSegment(temp_segment_metadata, sizeof(SegmentMetadata_t));
			msh_CloseSegmentHandle(temp_segment_handle);
		}
	}
	
	/* reset these to reduce confusion when debugging */
	segment_metadata->next_seg_num = -1;
	segment_metadata->prev_seg_num = -1;

#ifdef MSH_UNIX
	msh_WriteSegmentName(segment_name, seg_node->seg_info.seg_num);
	if(shm_unlink(segment_name) != 0)
	{
		ReadMexErrorWithCode(__FILE__, __LINE__, errno, "UnlinkError", "There was an error unlinking the segment");
	}
#endif
	
	/* sign the update with this process */
	g_shared_info->update_pid = g_local_info->this_pid;
	
	/* update number of vars in shared memory */
	g_shared_info->num_valid_segments -= 1;
	
	/* update the revision number to tell processes to update their segment lists */
	g_shared_info->rev_num += 1;
	
	msh_ReleaseProcessLock();
	
}

void msh_DetachSegmentList(SegmentList_t* seg_list)
{
	
	SegmentNode_t* curr_seg_node, * next_seg_node;
	for(curr_seg_node = seg_list->first; curr_seg_node != NULL; curr_seg_node = next_seg_node)
	{
		next_seg_node = curr_seg_node->next;
		msh_RemoveSegmentFromLocalList(curr_seg_node->parent_seg_list, curr_seg_node);
		if(msh_GetSegmentMetadata(curr_seg_node)->procs_tracking == 1)
		{
			msh_RemoveSegmentFromSharedList(curr_seg_node);
		}
		msh_DetachSegment(curr_seg_node);
	}
}


void msh_ClearSegmentList(SegmentList_t* seg_list)
{
	SegmentNode_t* curr_seg_node, * next_seg_node;
	for(curr_seg_node = seg_list->first; curr_seg_node != NULL; curr_seg_node = next_seg_node)
	{
		next_seg_node = curr_seg_node->next;
		
		msh_RemoveSegmentFromLocalList(curr_seg_node->parent_seg_list, curr_seg_node);
		msh_RemoveSegmentFromSharedList(curr_seg_node);
		msh_DetachSegment(curr_seg_node);
	}
}


void msh_UpdateSegmentTracking(SegmentList_t* seg_list)
{
	
	msh_segmentnumber_t curr_seg_num;
	SegmentNode_t* curr_seg_node, * prev_seg_node, * new_seg_node = NULL;
	SegmentList_t new_seg_list = {NULL, NULL, {0}};
	
	if(msh_IsUpdated())
	{
		/* don't do anything if this is true */
		return;
	}
	
	msh_AcquireProcessLock();
	
	/* mark these as locally invalid, but don't remove them yet so we don't waste time while locking */
	for(curr_seg_node = g_local_seg_list.first; curr_seg_node != NULL; curr_seg_node = curr_seg_node->next)
	{
		if(msh_GetSegmentMetadata(curr_seg_node)->is_invalid)
		{
			curr_seg_node->seg_info.is_local_invalid = TRUE;
		}
	}
	
	for(new_seg_list.num_segs = 0, curr_seg_num = g_shared_info->last_seg_num;
	    new_seg_list.num_segs < g_shared_info->num_valid_segments;
	    new_seg_list.num_segs++, curr_seg_num = msh_GetSegmentMetadata(new_seg_node)->prev_seg_num)
	{
		if((new_seg_node = msh_FindSegmentNode(&g_local_seg_list.seg_table, curr_seg_num)) == NULL || new_seg_node->seg_info.is_local_invalid)
		{
			new_seg_node = msh_OpenSegment(curr_seg_num);
		}
		
		/* take advantage of double link and only set pointers in one direction for now */
		new_seg_node->next = new_seg_list.first;
		new_seg_list.first = new_seg_node;
	}
	
	/* set this process as up to date */
	g_local_info->rev_num = g_shared_info->rev_num;
	
	msh_ReleaseProcessLock();
	
	/* detach segments marked as invalid during the lock */
	for(curr_seg_node = seg_list->last; curr_seg_node != NULL; curr_seg_node = prev_seg_node)
	{
		prev_seg_node = curr_seg_node->prev;
		msh_RemoveSegmentFromTable(&g_local_seg_list.seg_table, curr_seg_node);
		if(curr_seg_node->seg_info.is_local_invalid)
		{
			msh_DetachSegment(curr_seg_node);
		}
	}
	
	seg_list->first = new_seg_list.first;
	seg_list->num_segs = new_seg_list.num_segs;
	
	if(seg_list->num_segs > 0)
	{
		curr_seg_node = seg_list->first;
		curr_seg_node->prev = NULL;
		while(curr_seg_node->next != NULL)
		{
			msh_AddSegmentToTable(&g_local_seg_list.seg_table, curr_seg_node, g_local_seg_list.num_segs);
			curr_seg_node->next->prev = curr_seg_node;
			curr_seg_node->parent_seg_list = &g_local_seg_list;
			curr_seg_node = curr_seg_node->next;
		}
		msh_AddSegmentToTable(&g_local_seg_list.seg_table, curr_seg_node, g_local_seg_list.num_segs);
		curr_seg_node->parent_seg_list = &g_local_seg_list;
		seg_list->last = curr_seg_node;
	}
	else
	{
		seg_list->last = NULL;
	}
	
}


void msh_UpdateLatestSegment(SegmentList_t* seg_list)
{
	SegmentNode_t* latest_seg_node;
	if(g_shared_info->last_seg_num != -1 && !msh_IsUpdated())
	{
		msh_AcquireProcessLock();
		
		/* double check volatile condition */
		if(g_shared_info->last_seg_num != -1)
		{
			if((latest_seg_node = msh_FindSegmentNode(&seg_list->seg_table, g_shared_info->last_seg_num)) != NULL)
			{
				msh_ReleaseProcessLock();
				
				/* place the segment at the end of the list */
				msh_RemoveSegmentFromLocalList(seg_list, latest_seg_node);
				msh_AddSegmentToLocalList(seg_list, latest_seg_node);
			}
			else
			{
				latest_seg_node = msh_OpenSegment(g_shared_info->last_seg_num);
				msh_ReleaseProcessLock();
				
				msh_AddSegmentToLocalList(seg_list, latest_seg_node);
			}
		}
		else
		{
			msh_ReleaseProcessLock();
		}
	}
}


void msh_AddSegmentToLocalList(SegmentList_t* seg_list, SegmentNode_t* seg_node)
{
	
	/* this will be appended to the end so make sure next points to nothing */
	seg_node->next = NULL;
	
	/* always points to the end */
	seg_node->prev = seg_list->last;
	
	/* set new refs */
	if(seg_list->num_segs == 0)
	{
		seg_list->first = seg_node;
	}
	else
	{
		/* set list pointer */
		seg_list->last->next = seg_node;
	}
	
	/* place this variable at the last of the list */
	seg_list->last = seg_node;
	
	/* increment number of segments */
	seg_list->num_segs += 1;
	
	/* set the parent segment list */
	seg_node->parent_seg_list = seg_list;
	
	msh_AddSegmentToTable(&seg_list->seg_table, seg_node, seg_list->num_segs);
	
}


void msh_RemoveSegmentFromLocalList(SegmentList_t* seg_list, SegmentNode_t* seg_node)
{
	
	msh_RemoveSegmentFromTable(&seg_list->seg_table, seg_node);
	
	/* reset local pointers */
	if(seg_node->prev != NULL)
	{
		seg_node->prev->next = seg_node->next;
	}
	
	if(seg_node->next != NULL)
	{
		seg_node->next->prev = seg_node->prev;
	}
	
	/* fix pointers to the front and back */
	if(seg_list->first == seg_node)
	{
		seg_list->first = seg_node->next;
	}
	
	if(seg_list->last == seg_node)
	{
		seg_list->last = seg_node->prev;
	}
	
	/* decrement the number of segments now in case we crash when doing the unmapping */
	seg_list->num_segs -= 1;
	
}


void msh_CleanSegmentList(SegmentList_t* seg_list)
{
	SegmentNode_t* curr_seg_node, * next_seg_node;
	SegmentMetadata_t* curr_seg_metadata;
	
	if(g_shared_info->user_def.sharetype == msh_SHARETYPE_COPY && g_shared_info->user_def.will_gc && g_local_info->num_registered_objs == 0)
	{
		for(curr_seg_node = seg_list->first; curr_seg_node != NULL; curr_seg_node = next_seg_node)
		{
			next_seg_node = curr_seg_node->next;
			curr_seg_metadata = msh_GetSegmentMetadata(curr_seg_node);
			if(curr_seg_metadata->is_invalid)
			{
				msh_RemoveSegmentFromLocalList(curr_seg_node->parent_seg_list, curr_seg_node);
				msh_DetachSegment(curr_seg_node);
			}
			else if(curr_seg_node->var_node != NULL && msh_GetCrosslink(curr_seg_node->var_node->var) == NULL)
			{
				if(curr_seg_metadata->procs_using == 1)
				{
					/* this is the last process using this variable, so destroy the segment completely */
					msh_RemoveSegmentFromLocalList(curr_seg_node->parent_seg_list, curr_seg_node);
					msh_RemoveSegmentFromSharedList(curr_seg_node);
					msh_DetachSegment(curr_seg_node);
				}
				else
				{
					/* otherwise just take out the variable */
					msh_DestroyVariable(curr_seg_node->var_node);
				}
			}
		}
	}
	else
	{
		for(curr_seg_node = seg_list->first; curr_seg_node != NULL; curr_seg_node = next_seg_node)
		{
			next_seg_node = curr_seg_node->next;
			curr_seg_metadata = msh_GetSegmentMetadata(curr_seg_node);
			if(curr_seg_metadata->is_invalid)
			{
				msh_RemoveSegmentFromLocalList(curr_seg_node->parent_seg_list, curr_seg_node);
				msh_DetachSegment(curr_seg_node);
			}
		}
	}
	
}


/** static function definitions **/


static void msh_CreateSegmentWorker(SegmentInfo_t* seg_info_cache)
{
	SegmentMetadata_t* segment_metadata;

#ifdef MSH_WIN
	DWORD hi_sz, lo_sz;
#endif
	
	handle_t temp_handle;
	
	char_t segment_name[MSH_MAX_NAME_LEN];
	
	/* tracked segments must be up to date to get proper linking (will remove later
	 * when verified that this cannot happen) */
	
	if(g_shared_info->num_valid_segments >= SEG_NUM_MAX)
	{
		ReadMexError(__FILE__, __LINE__, "TooManyVariablesError", "Could not share the variable. Too many variables are currently being shared.");
	}
	
	/* create a unique new segment */
	seg_info_cache->seg_num = g_shared_info->last_seg_num;

#ifdef MSH_WIN
	
	/* NOTE: seg_sz must have been initialized before calling this */
	
#ifdef MSH_32BIT
	lo_sz = (DWORD)seg_info_cache->seg_sz;
	hi_sz = (DWORD)0;
#else
	/* split the 64-bit size */
	lo_sz = (DWORD)(seg_info_cache->seg_sz & 0xFFFFFFFFL);
	hi_sz = (DWORD)((seg_info_cache->seg_sz >> 32u) & 0xFFFFFFFFL);
#endif
	
	/* the targeted segment number is not guaranteed to be available, so keep retrying */
	do
	{
		/* change the file name */
		seg_info_cache->seg_num = (seg_info_cache->seg_num == SEG_NUM_MAX)? 0 : seg_info_cache->seg_num + 1;
		msh_WriteSegmentName(segment_name, seg_info_cache->seg_num);
		
		SetLastError(0);
		temp_handle = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, hi_sz, lo_sz, segment_name);
		if(temp_handle == NULL)
		{
			ReadMexError(__FILE__, __LINE__, "CreateFileError", "Error creating the file mapping (Error Number %u).", GetLastError());
		}
		else if(GetLastError() == ERROR_ALREADY_EXISTS)
		{
			if(CloseHandle(temp_handle) == 0)
			{
				ReadMexError(__FILE__, __LINE__, "CloseHandleError", "Error closing the file handle (Error Number %u).", GetLastError());
			}
		}
	} while(GetLastError() == ERROR_ALREADY_EXISTS);
	seg_info_cache->handle = temp_handle;
	seg_info_cache->is_init = TRUE;
	
	seg_info_cache->shared_memory_ptr = MapViewOfFile(seg_info_cache->handle, FILE_MAP_ALL_ACCESS, 0, 0, seg_info_cache->seg_sz);
	if(seg_info_cache->shared_memory_ptr == NULL)
	{
		ReadMexError(__FILE__, __LINE__, "MapDataSegError", "Could not map the data memory segment (Error number %u)", GetLastError());
	}
	seg_info_cache->is_mapped = TRUE;

#else
	
	/* find an available shared memory file name */
	do
	{
		/* change the file name */
		seg_info_cache->seg_num = (seg_info_cache->seg_num == SEG_NUM_MAX)? 0 : seg_info_cache->seg_num + 1;
		msh_WriteSegmentName(segment_name, seg_info_cache->seg_num);
		
		/* errno is not set unless the function fails, so reset it each time */
		errno = 0;
		temp_handle = shm_open(segment_name, O_RDWR | O_CREAT | O_EXCL, g_shared_info->user_def.security);
		if(temp_handle == -1 && errno != EEXIST)
		{
			ReadMexErrorWithCode(__FILE__, __LINE__, errno, "CreateError", "There was an error creating the segment");
		}
	} while(errno == EEXIST);
	seg_info_cache->handle = temp_handle;
	seg_info_cache->is_init = TRUE;
	
	/* change the map size */
	if(ftruncate(seg_info_cache->handle, seg_info_cache->seg_sz) != 0)
	{
		ReadMexErrorWithCode(__FILE__, __LINE__, errno, "TruncateError", "There was an error truncating the segment");
	}
	
	/* map the shared memory */
	seg_info_cache->shared_memory_ptr = mmap(NULL, seg_info_cache->seg_sz, PROT_READ | PROT_WRITE, MAP_SHARED, seg_info_cache->handle, 0);
	if(seg_info_cache->shared_memory_ptr == MAP_FAILED)
	{
		ReadMexErrorWithCode(__FILE__, __LINE__, errno, "MmapError", "There was an error memory mapping the segment");
	}
	seg_info_cache->is_mapped = TRUE;

#endif
	
	seg_info_cache->is_local_invalid = FALSE;
	
	segment_metadata = (SegmentMetadata_t*)seg_info_cache->shared_memory_ptr;
	
	/* set the size of the segment */
	segment_metadata->seg_sz = seg_info_cache->seg_sz;
	
	/* number of processes with variables instantiated using this segment */
	segment_metadata->procs_using = 0;
	
	/* number of processes with a handle on this segment */
	segment_metadata->procs_tracking = 1;
	
	/* make sure that this segment has been used at least once before freeing it */
	segment_metadata->is_used = FALSE;
	
	/* indicates if the segment should be deleted by all processes */
	segment_metadata->is_invalid = FALSE;
	
	/* refer to nothing since this is the end of the list */
	segment_metadata->next_seg_num = -1;
	
}


static void msh_OpenSegmentWorker(SegmentInfo_t* seg_info_cache)
{
	SegmentMetadata_t* temp_map;
	
	seg_info_cache->handle = msh_OpenSegmentHandle(seg_info_cache->seg_num);
	seg_info_cache->is_init = TRUE;
	
	/* make a temporary map to get the segment size */
	temp_map = msh_MapSegment(seg_info_cache->handle, sizeof(SegmentMetadata_t));
	
	/* get the segment size */
	seg_info_cache->seg_sz = temp_map->seg_sz;
	
	/* unmap the temporary map */
	msh_UnmapSegment(temp_map, sizeof(SegmentMetadata_t));
	
	seg_info_cache->shared_memory_ptr = msh_MapSegment(seg_info_cache->handle, seg_info_cache->seg_sz);
	seg_info_cache->is_mapped = TRUE;
	
	seg_info_cache->is_local_invalid = FALSE;
	
	/* tell everyone else that another process is tracking this */
	msh_AtomicIncrement(&((SegmentMetadata_t*)seg_info_cache->shared_memory_ptr)->procs_tracking);
	
}


static handle_t msh_OpenSegmentHandle(msh_segmentnumber_t seg_num)
{
	
	handle_t ret_handle;
	
	char_t segment_name[MSH_MAX_NAME_LEN];
	
	/* update the segment name */
	msh_WriteSegmentName(segment_name, seg_num);

#ifdef MSH_WIN
	/* get the new file handle */
	ret_handle = OpenFileMapping(FILE_MAP_ALL_ACCESS, TRUE, segment_name);
	if(ret_handle == NULL)
	{
		ReadMexErrorWithCode(__FILE__, __LINE__, GetLastError(), "OpenFileError", "Error opening the file mapping.");
	}
#else
	ret_handle = shm_open(segment_name, O_RDWR, g_shared_info->user_def.security);
	if(ret_handle == -1)
	{
		ReadMexErrorWithCode(__FILE__, __LINE__, errno, "OpenError", "There was an error opening the segment");
	}
#endif

	return ret_handle;

}


static void* msh_MapSegment(handle_t segment_handle, size_t map_sz)
{
	
	void* seg_ptr;
	
#ifdef MSH_WIN
	/* get the new file handle */
	/* map the metadata to get the size of the segment */
	seg_ptr = MapViewOfFile(segment_handle, FILE_MAP_ALL_ACCESS, 0, 0, map_sz);
	if(seg_ptr == NULL)
	{
		ReadMexError(__FILE__, __LINE__, "MappingError", "Could not fetch the memory segment (Error number: %d).", GetLastError());
	}
#else
	seg_ptr = mmap(NULL, map_sz, PROT_READ | PROT_WRITE, MAP_SHARED, segment_handle, 0);
	if(seg_ptr == MAP_FAILED)
	{
		ReadMexErrorWithCode(__FILE__, __LINE__, errno, "MmapError", "There was an error memory mapping the segment");
	}
#endif

	return seg_ptr;

}

static void msh_UnmapSegment(void* segment_pointer, size_t map_sz)
{
#ifdef MSH_WIN
	if(UnmapViewOfFile(segment_pointer) == 0)
	{
		ReadMexErrorWithCode(__FILE__, __LINE__, GetLastError(), "UnmapFileError", "Error unmapping the file.");
	}
#else
	if(munmap(segment_pointer, map_sz) != 0)
	{
		ReadMexErrorWithCode(__FILE__, __LINE__, errno, "MunmapError", "There was an error unmapping the segment");
	}
#endif
}

static void msh_CloseSegmentHandle(handle_t segment_handle)
{
#ifdef MSH_WIN
	if(CloseHandle(segment_handle) == 0)
	{
		ReadMexErrorWithCode(__FILE__, __LINE__, GetLastError(), "CloseHandleError", "Error closing the data file handle.");
	}
#else
	if(close(segment_handle) == -1)
	{
		ReadMexErrorWithCode(__FILE__, __LINE__, errno, "CloseHandleError", "Error closing the data file handle.");
	}
#endif
}


static SegmentNode_t* msh_CreateSegmentNode(SegmentInfo_t* seg_info_cache)
{
	SegmentNode_t* new_seg_node = mxMalloc(sizeof(SegmentNode_t));
	mexMakeMemoryPersistent(new_seg_node);
	
	new_seg_node->seg_info = *seg_info_cache;
	new_seg_node->var_node = NULL;
	new_seg_node->parent_seg_list = NULL;
	new_seg_node->hash_next = NULL;
	new_seg_node->prev = NULL;
	new_seg_node->next = NULL;
	
	return new_seg_node;
}


static void msh_DestroySegmentNode(SegmentNode_t* seg_node)
{
	mxFree(seg_node);
}
