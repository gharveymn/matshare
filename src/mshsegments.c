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
static void msh_CreateSegmentWorker(SegmentInfo_t* seg_info_cache, size_t data_size);


/**
 * Does the actual opening operation. Writes information on the segment to seg_info_cache.
 * to be used immediately after.
 *
 * @note Interacts with but does not modify the shared memory linked list.
 * @param seg_num The segment number of the segment to be opened.
 */
static void msh_OpenSegmentWorker(SegmentInfo_t* seg_info_cache, msh_segmentnumber_t seg_num);


/**
 * Create a new segment node.
 *
 * @note Completely local.
 * @param seg_info A struct containing information about a shared memory segment created either by OpenSegment or CreateSegment.
 * @return The newly allocated segment node.
 */
static SegmentNode_t* msh_CreateSegmentNode(SegmentInfo_t* seg_info_cache);

static void msh_DestroySegmentNode(SegmentNode_t* seg_node);

static void msh_InitializeSegmentInfo(SegmentInfo_t* seg_info);


/** public function definitions **/

SharedVariableHeader_t* msh_GetSegmentData(SegmentNode_t* seg_node)
{
	/* The raw pointer is only mapped if it is actually needed.
	 * This improves performance of functions only needing the
	 * metadata without effecting performance of other functions. */
	if(seg_node->seg_info.raw_ptr == NULL)
	{
		seg_node->seg_info.raw_ptr = msh_MapMemory(seg_node->seg_info.handle, seg_node->seg_info.total_segment_size);
	}
	return (SharedVariableHeader_t*)((byte_t*)seg_node->seg_info.raw_ptr + PadToAlignData(sizeof(SegmentMetadata_t)));
}

size_t msh_FindSegmentSize(size_t data_size)
{
	return PadToAlignData(sizeof(SegmentMetadata_t)) + data_size;
}

SegmentNode_t* msh_CreateSegment(size_t data_size)
{
	SegmentInfo_t seg_info_cache;
	msh_InitializeSegmentInfo(&seg_info_cache);
	msh_CreateSegmentWorker(&seg_info_cache, data_size);
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
	
	if(seg_node->var_node != NULL)
	{
		msh_RemoveVariableFromList(seg_node->var_node);
		if(msh_DestroyVariable(seg_node->var_node))
		{
			return;
		}
	}
	
	if(seg_node->seg_info.metadata != NULL)
	{
		/* lockfree */
#ifdef MSH_WIN
		if(msh_AtomicDecrement(&msh_GetSegmentMetadata(seg_node)->procs_tracking) == 0)
		{
			/* this is nilpotent, so if it's already been removed it's ok */
			msh_RemoveSegmentFromSharedList(seg_node);
		}
#else

		if(msh_GetCounterCount(&msh_GetSegmentMetadata(seg_node)->procs_tracking) == 1)
		{
			/* this is nilpotent, so if it's already been removed it's ok */
			msh_RemoveSegmentFromSharedList(seg_node);
		}

		if(msh_DecrementCounter(&msh_GetSegmentMetadata(seg_node)->procs_tracking, TRUE))
		{
			msh_WriteSegmentName(segment_name, seg_node->seg_info.seg_num);
			if(shm_unlink(segment_name) != 0)
			{
				ReadMexErrorWithCode(__FILE__, __LINE__, errno, "UnlinkError", "There was an error unlinking the segment");
			}
			msh_SetCounterPost(&msh_GetSegmentMetadata(seg_node)->procs_tracking, TRUE);
		}
#endif
		
		msh_UnmapMemory(seg_node->seg_info.metadata, sizeof(SegmentMetadata_t));
		seg_node->seg_info.metadata = NULL;
		
	}
	
	if(seg_node->seg_info.raw_ptr != NULL)
	{
		msh_UnmapMemory(seg_node->seg_info.raw_ptr, seg_node->seg_info.total_segment_size);
		seg_node->seg_info.raw_ptr = NULL;
	}
	
	if(seg_node->seg_info.handle != MSH_INVALID_HANDLE)
	{
		msh_CloseSharedMemory(seg_node->seg_info.handle);
		seg_node->seg_info.handle = MSH_INVALID_HANDLE;
	}
	
	msh_DestroySegmentNode(seg_node);
	
}


void msh_AddSegmentToSharedList(SegmentNode_t* seg_node)
{
	SegmentNode_t* last_seg_node;
	SegmentMetadata_t* segment_metadata = msh_GetSegmentMetadata(seg_node);
	
	msh_AcquireProcessLock();
	
	if(g_shared_info->user_defined.sharetype == msh_SHARETYPE_OVERWRITE)
	{
		msh_ClearSharedSegments(seg_node->parent_seg_list);
	}
	
	/* check whether to set this as the first segment number */
	if(g_shared_info->num_valid_segments == 0)
	{
		g_shared_info->first_seg_num = seg_node->seg_info.seg_num;
	}
	else
	{
		if((last_seg_node = msh_FindSegmentNode(&seg_node->parent_seg_list->seg_table, g_shared_info->last_seg_num)) == NULL)
		{
			/* track the new segment since the mxMalloc should be cheaper than upmapping and closing the handle */
			last_seg_node = msh_OpenSegment(g_shared_info->last_seg_num);
			msh_AddSegmentToList(seg_node->parent_seg_list, last_seg_node);
		}
		msh_GetSegmentMetadata(last_seg_node)->next_seg_num = seg_node->seg_info.seg_num;
	}
	
	/* set reference to previous back of list */
	segment_metadata->prev_seg_num = g_shared_info->last_seg_num;
	
	/* update the last segment number */
	g_shared_info->last_seg_num = seg_node->seg_info.seg_num;
	
	/* update number of vars in shared memory */
	g_shared_info->num_valid_segments += 1;
	
	/* sign the update with this process */
	g_shared_info->update_pid = g_local_info.this_pid;
	
	/* update the revision number to indicate other processes to retrieve new segments */
	g_shared_info->rev_num += 1;
	
	msh_ReleaseProcessLock();
	
}


void msh_RemoveSegmentFromSharedList(SegmentNode_t* seg_node)
{
	
	SegmentNode_t* prev_seg_node,* next_seg_node;
	SegmentMetadata_t* segment_metadata = msh_GetSegmentMetadata(seg_node);
	
	if(segment_metadata->is_invalid == TRUE)
	{
		return;
	}
	
	msh_AcquireProcessLock();
	
	/* double check volatile flag */
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
		if(seg_node->parent_seg_list == NULL)
		{
			/* this probably won't happen, but for robustness this will remain */
			prev_seg_node = msh_OpenSegment(segment_metadata->prev_seg_num);
			msh_GetSegmentMetadata(prev_seg_node)->next_seg_num = segment_metadata->next_seg_num;
			msh_DetachSegment(prev_seg_node);
		}
		else
		{
			if((prev_seg_node = msh_FindSegmentNode(&seg_node->parent_seg_list->seg_table, segment_metadata->prev_seg_num)) == NULL)
			{
				/* track the new segment since the mxMalloc should be cheaper than upmapping and closing the handle */
				prev_seg_node = msh_OpenSegment(segment_metadata->prev_seg_num);
				msh_AddSegmentToList(seg_node->parent_seg_list, prev_seg_node);
			}
			msh_GetSegmentMetadata(prev_seg_node)->next_seg_num = segment_metadata->next_seg_num;
		}
		
	}
	
	if(seg_node->seg_info.seg_num == g_shared_info->last_seg_num)
	{
		g_shared_info->last_seg_num = segment_metadata->prev_seg_num;
	}
	else
	{
		if(seg_node->parent_seg_list == NULL)
		{
			/* this probably won't happen, but for robustness this will remain */
			next_seg_node = msh_OpenSegment(segment_metadata->next_seg_num);
			msh_GetSegmentMetadata(next_seg_node)->prev_seg_num = segment_metadata->prev_seg_num;
			msh_DetachSegment(next_seg_node);
		}
		else
		{
			if((next_seg_node = msh_FindSegmentNode(&seg_node->parent_seg_list->seg_table, segment_metadata->next_seg_num)) == NULL)
			{
				/* track the new segment since the mxMalloc should be cheaper than upmapping and closing the handle */
				next_seg_node = msh_OpenSegment(segment_metadata->next_seg_num);
				msh_AddSegmentToList(seg_node->parent_seg_list, next_seg_node);
			}
			msh_GetSegmentMetadata(next_seg_node)->prev_seg_num = segment_metadata->prev_seg_num;
		}
	}
	
	/* reset these to reduce confusion when debugging */
	segment_metadata->next_seg_num = MSH_INVALID_SEG_NUM;
	segment_metadata->prev_seg_num = MSH_INVALID_SEG_NUM;
	
	/* sign the update with this process */
	g_shared_info->update_pid = g_local_info.this_pid;
	
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
		msh_RemoveSegmentFromList(curr_seg_node);
		msh_DetachSegment(curr_seg_node);
	}
}


void msh_ClearSharedSegments(SegmentList_t* seg_list)
{
	SegmentNode_t* curr_seg_node;
	while(g_shared_info->first_seg_num != MSH_INVALID_SEG_NUM)
	{
		if((curr_seg_node = msh_FindSegmentNode(&seg_list->seg_table, g_shared_info->first_seg_num)) == NULL)
		{
			curr_seg_node = msh_OpenSegment(g_shared_info->first_seg_num);
			msh_AddSegmentToList(seg_list, curr_seg_node);
		}
		msh_RemoveSegmentFromSharedList(curr_seg_node);
		msh_RemoveSegmentFromList(curr_seg_node);
		msh_DetachSegment(curr_seg_node);
	}
}


void msh_UpdateSegmentTracking(SegmentList_t* seg_list)
{
	
	msh_segmentnumber_t curr_seg_num;
	SegmentNode_t* curr_seg_node,* next_seg_node, * new_seg_node,* new_front = NULL;
	
	if(msh_IsUpdated())
	{
		/* don't do anything if this is true */
		return;
	}
	
	msh_AcquireProcessLock();
	
	for(curr_seg_num = g_shared_info->first_seg_num;
	    curr_seg_num != MSH_INVALID_SEG_NUM;
	    curr_seg_num = msh_GetSegmentMetadata(new_seg_node)->next_seg_num)
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
	
	msh_ReleaseProcessLock();
	
	/* detach the nodes that aren't in newly linked*/
	for(curr_seg_node = seg_list->first; curr_seg_node != new_front; curr_seg_node = next_seg_node)
	{
		next_seg_node = curr_seg_node->next;
		msh_RemoveSegmentFromList(curr_seg_node);
		msh_DetachSegment(curr_seg_node);
	}
	
}


void msh_UpdateLatestSegment(SegmentList_t* seg_list)
{
	SegmentNode_t* latest_seg_node;
	if(g_shared_info->last_seg_num != MSH_INVALID_SEG_NUM && !msh_IsUpdated())
	{
		msh_AcquireProcessLock();
		
		/* double check volatile flag */
		if(g_shared_info->last_seg_num != MSH_INVALID_SEG_NUM)
		{
			if((latest_seg_node = msh_FindSegmentNode(&seg_list->seg_table, g_shared_info->last_seg_num)) == NULL)
			{
				latest_seg_node = msh_OpenSegment(g_shared_info->last_seg_num);
				msh_ReleaseProcessLock();
				
				msh_AddSegmentToList(seg_list, latest_seg_node);
			}
			else
			{
				msh_ReleaseProcessLock();
				
				/* place the segment at the end of the list */
				msh_PlaceSegmentAtEnd(latest_seg_node);
			}
		}
		else
		{
			msh_ReleaseProcessLock();
		}
	}
}


void msh_AddSegmentToList(SegmentList_t* seg_list, SegmentNode_t* seg_node)
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


void msh_PlaceSegmentAtEnd(SegmentNode_t* seg_node)
{
	/* if it's already at the end do nothing */
	if(seg_node->parent_seg_list->last == seg_node)
	{
		return;
	}
	
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
	if(seg_node->parent_seg_list->first == seg_node)
	{
		seg_node->parent_seg_list->first = seg_node->next;
	}
	
	/* set up links at end of the list */
	seg_node->parent_seg_list->last->next = seg_node;
	seg_node->prev = seg_node->parent_seg_list->last;
	
	/* reset the next pointer and place at the end of the list */
	seg_node->next = NULL;
	seg_node->parent_seg_list->last = seg_node;
	
	
}


void msh_RemoveSegmentFromList(SegmentNode_t* seg_node)
{
	
	if(seg_node->parent_seg_list == NULL)
	{
		return;
	}
	
	msh_RemoveSegmentFromTable(&seg_node->parent_seg_list->seg_table, seg_node);
	
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
	if(seg_node->parent_seg_list->first == seg_node)
	{
		seg_node->parent_seg_list->first = seg_node->next;
	}
	
	if(seg_node->parent_seg_list->last == seg_node)
	{
		seg_node->parent_seg_list->last = seg_node->prev;
	}
	
	seg_node->parent_seg_list->num_segs -= 1;
	
	
	seg_node->next = NULL;
	seg_node->prev = NULL;
	seg_node->parent_seg_list = NULL;
	
}


void msh_CleanSegmentList(SegmentList_t* seg_list)
{
	SegmentNode_t* curr_seg_node, * next_seg_node;
	SegmentMetadata_t* curr_seg_metadata;
	
	if(g_shared_info->user_defined.sharetype == msh_SHARETYPE_COPY && g_shared_info->user_defined.will_gc && g_local_info.num_registered_objs == 0)
	{
		for(curr_seg_node = seg_list->first; curr_seg_node != NULL; curr_seg_node = next_seg_node)
		{
			next_seg_node = curr_seg_node->next;
			curr_seg_metadata = msh_GetSegmentMetadata(curr_seg_node);
			if(curr_seg_metadata->is_invalid)
			{
				msh_RemoveSegmentFromList(curr_seg_node);
				msh_DetachSegment(curr_seg_node);
			}
			else if(curr_seg_node->var_node != NULL && msh_GetCrosslink(curr_seg_node->var_node->var) == NULL)
			{
				msh_RemoveVariableFromList(curr_seg_node->var_node);
				msh_DestroyVariable(curr_seg_node->var_node);
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
				msh_RemoveSegmentFromList(curr_seg_node);
				msh_DetachSegment(curr_seg_node);
			}
		}
	}
	
}


/** static function definitions **/


static void msh_CreateSegmentWorker(SegmentInfo_t* seg_info_cache, size_t data_size)
{
	char_t segment_name[MSH_MAX_NAME_LEN];
	
	/* set the segment size */
	seg_info_cache->total_segment_size = msh_FindSegmentSize(data_size);
	
	if(g_shared_info->num_valid_segments >= MSH_SEG_NUM_MAX)
	{
		ReadMexError(__FILE__, __LINE__, "TooManyVariablesError", "Could not share the variable. Too many variables are currently being shared.");
	}
	
	/* create a unique new segment */
	seg_info_cache->seg_num = g_shared_info->last_seg_num;
	
	
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
	
	/* number of processes with variables instantiated using this segment */
	seg_info_cache->metadata->procs_using = 0;
	
	/* number of processes with a handle on this segment */
#ifdef MSH_WIN
	msh_AtomicIncrement(&seg_info_cache->metadata->procs_tracking);
#else
	msh_IncrementCounter(&seg_info_cache->metadata->procs_tracking);
#endif
	
	/* indicates if the segment should be deleted by all processes */
	seg_info_cache->metadata->is_invalid = FALSE;
	
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
	msh_AtomicIncrement(&seg_info_cache->metadata->procs_tracking);
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

#  ifdef MSH_32BIT
	DWORD lo_sz = (DWORD)segment_size;
	DWORD hi_sz = (DWORD)0;
#  else
	/* split the 64-bit size */
	DWORD lo_sz = (DWORD)(segment_size & 0xFFFFFFFFL);
	DWORD hi_sz = (DWORD)((segment_size >> 32u) & 0xFFFFFFFFL);
#  endif
	
	SetLastError(0);
	ret_handle = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, hi_sz, lo_sz, segment_name);
	if(ret_handle == NULL)
	{
		ReadMexErrorWithCode(__FILE__, __LINE__, GetLastError(), "CreateFileError", "Error creating the file mapping.");
	}
	else if(GetLastError() == ERROR_ALREADY_EXISTS)
	{
		if(CloseHandle(ret_handle) == 0)
		{
			ReadMexErrorWithCode(__FILE__, __LINE__, GetLastError(), "CloseHandleError", "Error closing the file handle.");
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
			ReadMexErrorWithCode(__FILE__, __LINE__, errno, "CreateError", "There was an error creating the segment");
		}
		return MSH_INVALID_HANDLE;
	}
	
	/* set the segment size */
	if(ftruncate(ret_handle, segment_size) != 0)
	{
		ReadMexErrorWithCode(__FILE__, __LINE__, errno, "TruncateError", "There was an error truncating the segment");
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
		ReadMexErrorWithCode(__FILE__, __LINE__, GetLastError(), "OpenFileError", "Error opening the file mapping.");
	}
#else
	ret_handle = shm_open(segment_name, O_RDWR, g_shared_info->user_defined.security);
	if(ret_handle == -1)
	{
		ReadMexErrorWithCode(__FILE__, __LINE__, errno, "OpenError", "There was an error opening the segment");
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
		ReadMexErrorWithCode(__FILE__, __LINE__, GetLastError(), "MemoryMappingError", "There was an error memory mapping the segment");
	}
#else
	seg_ptr = mmap(NULL, map_sz, PROT_READ | PROT_WRITE, MAP_SHARED, segment_handle, 0);
	if(seg_ptr == MAP_FAILED)
	{
		ReadMexErrorWithCode(__FILE__, __LINE__, errno, "MemoryMappingError", "There was an error memory mapping the segment");
	}
#endif

	return seg_ptr;

}

void msh_UnmapMemory(void* segment_pointer, size_t map_sz)
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

void msh_CloseSharedMemory(handle_t segment_handle)
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


static void msh_InitializeSegmentInfo(SegmentInfo_t* seg_info)
{
	seg_info->raw_ptr = NULL;
	seg_info->metadata = NULL;
	seg_info->total_segment_size = 0;
	seg_info->handle = MSH_INVALID_HANDLE;
	seg_info->seg_num = -1;
}
