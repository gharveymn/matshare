#include "headers/mshlists.h"
#include "headers/matlabutils.h"


extern void WriteSegmentName(char name_buffer[MSH_MAX_NAME_LEN], msh_segmentnumber_t seg_num);

/**
 * Used for caching segment information in this file.
 * SegmentInfo_t should only be found within SegmentNode_t elsewhere.
 */
static SegmentInfo_t s_seg_info_cache;


/**
 * Does the actual segment creation operation. Write information on the segment
 * to seg_info_cache to be used immediately after.
 *
 * @note Modifies the shared memory linked list.
 * @param seg_sz The size of the segment to be opened.
 */
static void CreateSegment_(size_t seg_sz);


/**
 * Does the actual opening operation. Writes information on the segment to seg_info_cache.
 * to be used immediately after.
 *
 * @note Interacts with but does not modify the shared memory linked list.
 * @param seg_num The segment number of the segment to be opened.
 */
static void OpenSegment_(msh_segmentnumber_t seg_num);


/**
 * Creates a new segment node with the given segment info and adds it to the segment list specified.
 *
 * @param seg_list The segment list to track the new segment.
 * @param seg_info A struct containing information about a shared memory segment created either by OpenSegment or CreateSegment.
 * @return The newly created segment node.
 */
static SegmentNode_t* TrackSegment(SegmentList_t* seg_list);


/**
 * Create a new segment node.
 *
 * @note Completely local.
 * @param seg_info A struct containing information about a shared memory segment created either by OpenSegment or CreateSegment.
 * @return The newly allocated segment node.
 */
static SegmentNode_t* CreateSegmentNode(void);


/**
 * Add a segment node to the specified segment list.
 *
 * @note Completely local.
 * @param seg_list The segment list which the segment node will be appended to.
 * @param seg_node The segment node to be appended.
 */
static void AddSegmentNode(SegmentList_t* seg_list, SegmentNode_t* seg_node);


/**
 * Removes a segment node from the specified segment list.
 *
 * @note Completely local.
 * @param seg_list The segment list which the segment node will be removed from.
 * @param seg_node The segment to be removed.
 */
static void RemoveSegmentNode(SegmentList_t* seg_list, SegmentNode_t* seg_node);


/**
 * Tracks the mxArray by encapsulating it in a variable node, adding it to the variable list
 * specified and associating it to the segment node specified.
 *
 * @note Completely local.
 * @param var_list The variable list which will track the variable node containing the mxArray.
 * @param seg_node The segment which holds the mxArray data.
 * @param new_var The mxArray to be tracked.
 * @return The new variable node.
 */
static VariableNode_t* TrackVariable(VariableList_t* var_list, SegmentNode_t* seg_node, mxArray* new_var);


/**
 * Creates a new variable node for the specified mxArray and segment node.
 *
 * @note Completely local.
 * @param seg_node The segment node linked to this variable node.
 * @param new_var The mxArray to be tracked.
 * @return The new variable node.
 */
static VariableNode_t* CreateVariableNode(SegmentNode_t* seg_node, mxArray* new_var);


/**
 * Adds a new variable node to the specified variable list.
 *
 * @note Completely local.
 * @param var_list The variable list to which the variable node will be appended.
 * @param var_node The variable node to append.
 */
static void AddVariableNode(VariableList_t* var_list, VariableNode_t* var_node);


/**
 * Removes a new variable node from the specified variable list.
 *
 * @note Completely local.
 * @param var_list The variable list from which the variable node will be removed.
 * @param var_node The variable node to removed.
 */
static void RemoveVariableNode(VariableList_t* var_list, VariableNode_t* var_node);


/** public functions **/


SegmentMetadata_t* msh_GetSegmentMetadata(SegmentNode_t* seg_node)
{
	return seg_node->seg_info.shared_memory_ptr;
}

SharedVariableHeader_t* MshGetSegmentData(SegmentNode_t* seg_node)
{
	return (SharedVariableHeader_t*)((byte_t*)seg_node->seg_info.shared_memory_ptr + PadToAlign(sizeof(SegmentMetadata_t)));
}


size_t msh_FindSegmentSize(const mxArray* in_var)
{
	return msh_FindSharedSize(in_var) + PadToAlign(sizeof(SegmentMetadata_t));
}


SegmentNode_t* CreateSegment(SegmentList_t* seg_list, size_t seg_sz)
{
	
	CreateSegment_(seg_sz);
	return TrackSegment(seg_list);
}


SegmentNode_t* OpenSegment(SegmentList_t* seg_list, msh_segmentnumber_t seg_num)
{
	OpenSegment_(seg_num);
	return TrackSegment(seg_list);
}


void DetachSegment(SegmentNode_t* seg_node)
{
#ifdef MSH_UNIX
	bool_t is_last_tracking = FALSE;
#endif
	
	if(seg_node->var_node != NULL)
	{
		DestroyVariable(seg_node->var_node);
	}
	
	if(seg_node->seg_info.is_mapped)
	{
		msh_GetSegmentMetadata(seg_node)->procs_tracking -= 1;

#ifdef MSH_WIN
		if(UnmapViewOfFile((void*)seg_node->seg_info.shared_memory_ptr) == 0)
		{
			ReadMexError("UnmapFileError", "Error unmapping the data file (Error Number %u)", GetLastError());
		}
#else
		/* check if this process will unlink the shared memory */
		is_last_tracking = (MshGetSegmentMetadata(seg_node)->procs_tracking == 0);
		if(munmap(seg_node->seg_info.shared_memory_ptr, seg_node->seg_info.seg_sz) != 0)
		{
			ReadMunmapError(errno);
		}
#endif
		
		seg_node->seg_info.is_mapped = FALSE;
	}
	
	if(seg_node->seg_info.is_init)
	{
#ifdef MSH_WIN
		if(CloseHandle(seg_node->seg_info.handle) == 0)
		{
			ReadMexError("CloseHandleError", "Error closing the data file handle (Error Number %u)", GetLastError());
		}
#else
		if(is_last_tracking)
		{
			if(shm_unlink(seg_node->seg_info.name) != 0)
			{
				ReadShmUnlinkError(errno);
			}
		}
#endif
		seg_node->seg_info.is_init = FALSE;
	}
	
	/* remove tracking for this segment */
	RemoveSegmentNode(seg_node->parent_seg_list, seg_node);
	
}


void DestroySegment(SegmentNode_t* seg_node)
{
#ifdef MSH_UNIX
	bool_t is_last_tracking = FALSE;
#endif
	
	/* tracked segments must be up to date to get proper linking (will remove later
	  * when verified that this cannot happen) */
	if(!MshIsUpdated())
	{
		ReadMexError("OutOfDateError", "Could not create a segment because matshare was out of date.");
	}
	
	if(seg_node->var_node != NULL)
	{
		DestroyVariable(seg_node->var_node);
	}
	
	/* if this segment has already been invalidated, all the shared list information should already be set */
	if(msh_GetSegmentMetadata(seg_node)->is_invalid)
	{
		DetachSegment(seg_node);
		return;
	}
	
	if(seg_node->seg_info.is_mapped)
	{
		/* signal that this segment is to be freed by all processes */
		msh_GetSegmentMetadata(seg_node)->is_invalid = TRUE;
		
		if(msh_GetSegmentMetadata(seg_node)->seg_num == g_shared_info->first_seg_num)
		{
			g_shared_info->first_seg_num = msh_GetSegmentMetadata(seg_node)->next_seg_num;
		}
		
		if(msh_GetSegmentMetadata(seg_node)->seg_num == g_shared_info->last_seg_num)
		{
			g_shared_info->last_seg_num = msh_GetSegmentMetadata(seg_node)->prev_seg_num;
		}
		
		if(seg_node->prev != NULL && seg_node->prev->seg_info.is_mapped)
		{
			msh_GetSegmentMetadata(seg_node->prev)->next_seg_num = msh_GetSegmentMetadata(seg_node)->next_seg_num;
		}
		
		if(seg_node->next != NULL && seg_node->next->seg_info.is_mapped)
		{
			msh_GetSegmentMetadata(seg_node->next)->prev_seg_num = msh_GetSegmentMetadata(seg_node)->prev_seg_num;
		}
		
		msh_GetSegmentMetadata(seg_node)->procs_tracking -= 1;

#ifdef MSH_WIN
		if(UnmapViewOfFile((void*)seg_node->seg_info.shared_memory_ptr) == 0)
		{
			ReadMexError("UnmapFileError", "Error unmapping the data file (Error Number %u)", GetLastError());
		}
#else
		/* check if this process will unlink the shared memory */
		is_last_tracking = (MshGetSegmentMetadata(seg_node)->procs_tracking == 0);
		if(munmap(seg_node->seg_info.shared_memory_ptr, seg_node->seg_info.seg_sz) != 0)
		{
			ReadMunmapError(errno);
		}
#endif
		
		seg_node->seg_info.is_mapped = FALSE;
	}
	
	if(seg_node->seg_info.is_init)
	{
#ifdef MSH_WIN
		if(CloseHandle(seg_node->seg_info.handle) == 0)
		{
			ReadMexError("CloseHandleError", "Error closing the data file handle (Error Number %u)", GetLastError());
		}
#else
		if(is_last_tracking)
		{
			if(shm_unlink(seg_node->seg_info.name) != 0)
			{
				ReadShmUnlinkError(errno);
			}
		}
#endif
		seg_node->seg_info.is_init = FALSE;
	}
	
	/* remove tracking for this (now invalid) segment */
	RemoveSegmentNode(seg_node->parent_seg_list, seg_node);
	
	g_shared_info->num_valid_segments -= 1;
	
}


VariableNode_t* CreateVariable(VariableList_t* var_list, SegmentNode_t* seg_node)
{
	mxArray* new_var;
	
	msh_FetchVariable(MshGetSegmentData(seg_node), &new_var);
	mexMakeArrayPersistent(new_var);
	
	msh_GetSegmentMetadata(seg_node)->is_used = TRUE;
	msh_GetSegmentMetadata(seg_node)->procs_using += 1;
	
	return TrackVariable(var_list, seg_node, new_var);
}


void DestroyVariable(VariableNode_t* var_node)
{
	/* NULL all of the Matlab pointers */
	msh_DetachVariable(var_node->var);
	mxDestroyArray(var_node->var);
	
	/* decrement number of processes using this variable */
	msh_GetSegmentMetadata(var_node->seg_node)->procs_using -= 1;
	
	/* tell the segment to stop tracking this variable */
	var_node->seg_node->var_node = NULL;
	
	/* remove tracking for the destroyed variable */
	RemoveVariableNode(var_node->parent_var_list, var_node);
	
	mxFree(var_node);
}


void ClearVariableList(VariableList_t* var_list)
{
	VariableNode_t* curr_var_node, * next_var_node;
	
	curr_var_node = var_list->first;
	while(curr_var_node != NULL)
	{
		next_var_node = curr_var_node->next;
		DestroyVariable(curr_var_node);
		curr_var_node = next_var_node;
	}
}


void DetachSegmentList(SegmentList_t* seg_list)
{
	/* for debugging; this function should only run if everything is up to date
	 * otherwise we'll have some invalid segment numbers */
	if(!MshIsUpdated())
	{
		ReadMexError("OutOfDateError", "Could not create a segment because matshare was out of date.");
	}
	
	SegmentNode_t* curr_seg_node, * next_seg_node;
	curr_seg_node = seg_list->first;
	while(curr_seg_node != NULL)
	{
		next_seg_node = curr_seg_node->next;
		if(msh_GetSegmentMetadata(curr_seg_node)->procs_tracking == 1)
		{
			DestroySegment(curr_seg_node);
		}
		else
		{
			DetachSegment(curr_seg_node);
		}
		curr_seg_node = next_seg_node;
	}
}


void ClearSegmentList(SegmentList_t* seg_list)
{
	SegmentNode_t* curr_seg_node, * next_seg_node;
	curr_seg_node = seg_list->first;
	while(curr_seg_node != NULL)
	{
		next_seg_node = curr_seg_node->next;
		DestroySegment(curr_seg_node);
		curr_seg_node = next_seg_node;
	}
}


void UpdateSharedSegments(void)
{
	
	if(MshIsUpdated())
	{
		/* don't do anything if this is true */
		return;
	}
	
	msh_segmentnumber_t curr_seg_num;
	bool_t is_fetched;
	
	SegmentNode_t* curr_seg_node, * next_seg_node, * new_seg_node = NULL;
	SegmentList_t new_seg_list = {NULL, NULL, 0};
	
	curr_seg_node = g_local_seg_list.first;
	while(curr_seg_node != NULL)
	{
		next_seg_node = curr_seg_node->next;
		
		/* Check if the data segment is ready for deletion.
		 * Segment should not be hooked into the shared linked list */
		if(msh_GetSegmentMetadata(curr_seg_node)->is_invalid)
		{
			DetachSegment(curr_seg_node);
		}
		
		curr_seg_node = next_seg_node;
	}
	
	/* construct a new list of segments, reusing what we can */
	for(new_seg_list.num_segs = 0, curr_seg_num = g_shared_info->first_seg_num;
	    new_seg_list.num_segs < g_shared_info->num_valid_segments;
	    new_seg_list.num_segs++, curr_seg_num = msh_GetSegmentMetadata(new_seg_node)->next_seg_num)
	{
		
		curr_seg_node = g_local_seg_list.first;
		is_fetched = FALSE;
		while(curr_seg_node != NULL)
		{
			
			/* check if the current segment has been fetched */
			if(msh_GetSegmentMetadata(curr_seg_node)->seg_num == curr_seg_num)
			{
				/* hook the currently used node into the list */
				new_seg_node = curr_seg_node;
				
				is_fetched = TRUE;
				break;
			}
			curr_seg_node = curr_seg_node->next;
		}
		
		if(!is_fetched)
		{
			OpenSegment_(curr_seg_num);
			new_seg_node = CreateSegmentNode();
			new_seg_node->parent_seg_list = &g_local_seg_list;
		}
		
		/* take advantage of double link and only set pointers in one direction for now for a split structure*/
		new_seg_node->prev = new_seg_list.last;
		new_seg_list.last = new_seg_node;
		
	}
	
	/* set the new pointers in place */
	if(new_seg_list.num_segs > 0)
	{
		curr_seg_node = new_seg_list.last;
		while(curr_seg_node->prev != NULL)
		{
			curr_seg_node->prev->next = curr_seg_node;
			curr_seg_node = curr_seg_node->prev;
		}
		new_seg_list.first = curr_seg_node;
	}
	
	g_local_seg_list = new_seg_list;
	
	/* set this process as up to date */
	g_local_info->rev_num = g_shared_info->rev_num;
	
}


/** static functions **/


static void CreateSegment_(size_t seg_sz)
{
	SegmentMetadata_t* segment_metadata;

#ifdef MSH_WIN
	DWORD hi_sz, lo_sz;
	HANDLE temp_handle;
#else
	handle_t temp_handle;
#endif
	
	/* tracked segments must be up to date to get proper linking (will remove later
	 * when verified that this cannot happen) */
	if(!MshIsUpdated())
	{
		ReadMexError("OutOfDateError", "Could not create a segment because matshare was out of date.");
	}
	
	if(g_shared_info->num_valid_segments >= SEG_NUM_MAX)
	{
		ReadMexError("TooManyVariablesError", "Could not share the variable. Too many variables are currently being shared.");
	}
	
	/* create a unique new segment */
	msh_segmentnumber_t new_seg_num = g_shared_info->last_seg_num;
	
	/* set the size */
	s_seg_info_cache.seg_sz = seg_sz;

#ifdef MSH_WIN
	
	/* split the 64-bit size */
	lo_sz = (DWORD)(s_seg_info_cache.seg_sz & 0xFFFFFFFFL);
	hi_sz = (DWORD)((s_seg_info_cache.seg_sz >> 32u) & 0xFFFFFFFFL);
	
	do
	{
		/* change the file name */
		new_seg_num = (new_seg_num == SEG_NUM_MAX)? 0 : new_seg_num + 1;
		WriteSegmentName(s_seg_info_cache.name, new_seg_num);
		SetLastError(0);
		temp_handle = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, hi_sz, lo_sz, s_seg_info_cache.name);
		if(temp_handle == NULL)
		{
			ReadMexError("CreateFileError", "Error creating the file mapping (Error Number %u).", GetLastError());
		}
		else if(GetLastError() == ERROR_ALREADY_EXISTS)
		{
			if(CloseHandle(temp_handle) == 0)
			{
				ReadMexError("CloseHandleError", "Error closing the file handle (Error Number %u).", GetLastError());
			}
		}
	} while(GetLastError() == ERROR_ALREADY_EXISTS);
	s_seg_info_cache.handle = temp_handle;
	s_seg_info_cache.is_init = TRUE;
	
	s_seg_info_cache.shared_memory_ptr = MapViewOfFile(s_seg_info_cache.handle, FILE_MAP_ALL_ACCESS, 0, 0, s_seg_info_cache.seg_sz);
	if(s_seg_info_cache.shared_memory_ptr == NULL)
	{
		ReadMexError("MapDataSegError", "Could not map the data memory segment (Error number %u)", GetLastError());
	}
	s_seg_info_cache.is_mapped = TRUE;

#else
	
	/* find an available shared memory file name */
	errno = 0;
	do
	{
		/* change the file name */
		new_seg_num = (new_seg_num == SEG_NUM_MAX)? 0 : new_seg_num + 1;
		WriteSegmentName(s_seg_info_cache.name, new_seg_num);
		temp_handle = shm_open(s_seg_info_cache.name, O_RDWR | O_CREAT | O_EXCL, g_shared_info->security);
		if(temp_handle == -1)
		{
			if(errno != EEXIST)
			{
				ReadShmOpenError(errno);
			}
		}
	} while(errno == EEXIST);
	s_seg_info_cache.handle = temp_handle;
	s_seg_info_cache.is_init = TRUE;
	
	/* change the map size */
	if(ftruncate(s_seg_info_cache.handle, s_seg_info_cache.seg_sz) != 0)
	{
		ReleaseProcessLock();
		ReadFtruncateError(errno);
	}
	
	/* map the shared memory */
	s_seg_info_cache.shared_memory_ptr = mmap(NULL, s_seg_info_cache.seg_sz, PROT_READ | PROT_WRITE, MAP_SHARED, s_seg_info_cache.handle, 0);
	if(s_seg_info_cache.shared_memory_ptr == MAP_FAILED)
	{
		ReleaseProcessLock();
		ReadMmapError(errno);
	}
	s_seg_info_cache.is_mapped = TRUE;

#endif
	
	segment_metadata = (SegmentMetadata_t*)s_seg_info_cache.shared_memory_ptr;
	
	/* set the size of the segment */
	segment_metadata->seg_sz = seg_sz;
	
	/* number of processes with variables instantiated using this segment */
	segment_metadata->procs_using = 0;
	
	/* number of processes with a handle on this segment */
	segment_metadata->procs_tracking = 1;
	
	/* make sure that this segment has been used at least once before freeing it */
	segment_metadata->is_used = FALSE;
	
	/* indicates if the segment should be deleted by all processes */
	segment_metadata->is_invalid = FALSE;
	
	/* set the segment number */
	segment_metadata->seg_num = new_seg_num;
	
	/* refer to nothing since this is the end of the list */
	segment_metadata->next_seg_num = -1;
	
	/* check whether to set this as the first segment number */
	if(g_shared_info->num_valid_segments == 0)
	{
		g_shared_info->first_seg_num = new_seg_num;
		
		/* refer to nothing */
		segment_metadata->prev_seg_num = -1;
		
	}
	else
	{
		/* set reference in back of list to this segment */
		msh_GetSegmentMetadata(g_local_seg_list.last)->next_seg_num = new_seg_num;
		
		/* set reference to previous back of list */
		segment_metadata->prev_seg_num = g_shared_info->last_seg_num;
	}
	
	/* update the last segment number */
	g_shared_info->last_seg_num = new_seg_num;
	
	/* update number of vars in shared memory */
	g_shared_info->num_valid_segments += 1;
	
	/* update the revision number to indicate to retrieve new segments */
	g_shared_info->rev_num += 1;
	g_local_info->rev_num = g_shared_info->rev_num;
}


static void OpenSegment_(msh_segmentnumber_t seg_num)
{
	SegmentMetadata_t* temp_map;
	
	/* update the segment name */
	WriteSegmentName(s_seg_info_cache.name, seg_num);

#ifdef MSH_WIN
	/* get the new file handle */
	s_seg_info_cache.handle = OpenFileMapping(FILE_MAP_ALL_ACCESS, TRUE, s_seg_info_cache.name);
	if(s_seg_info_cache.handle == NULL)
	{
		ReadMexError("OpenFileError", "Error opening the file mapping (Error Number %u)", GetLastError());
	}
	s_seg_info_cache.is_init = TRUE;
	
	
	/* map the metadata to get the size of the segment */
	temp_map = MapViewOfFile(s_seg_info_cache.handle, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SegmentMetadata_t));
	if(temp_map == NULL)
	{
		ReadMexError("MappingError", "Could not fetch the memory segment (Error number: %d).", GetLastError());
	}
	else
	{
		/* get the segment size */
		s_seg_info_cache.seg_sz = temp_map->seg_sz;
	}
	
	/* unmap the temporary view */
	if(UnmapViewOfFile((void*)temp_map) == 0)
	{
		ReadMexError("UnmapFileError", "Error unmapping the file (Error Number %u)", GetLastError());
	}
	
	/* now map the whole thing */
	s_seg_info_cache.shared_memory_ptr = MapViewOfFile(s_seg_info_cache.handle, FILE_MAP_ALL_ACCESS, 0, 0, s_seg_info_cache.seg_sz);
	if(s_seg_info_cache.shared_memory_ptr == NULL)
	{
		ReadMexError("MappingError", "Could not fetch the memory segment (Error number: %d).", GetLastError());
	}
	s_seg_info_cache.is_mapped = TRUE;

#else
	/* get the new file handle */
	s_seg_info_cache.handle = shm_open(s_seg_info_cache.name, O_RDWR, g_shared_info->security);
	if(s_seg_info_cache.handle == -1)
	{
		ReadShmOpenError(errno);
	}
	s_seg_info_cache.is_init = TRUE;
	
	/* map the metadata to get the size of the segment */
	temp_map = mmap(NULL, sizeof(SegmentMetadata_t), PROT_READ | PROT_WRITE, MAP_SHARED, s_seg_info_cache.handle, 0);
	if(temp_map == MAP_FAILED)
	{
		ReadMmapError(errno);
	}
	
	/* get the segment size */
	s_seg_info_cache.seg_sz = temp_map->seg_sz;
	
	/* unmap the temporary map */
	if(munmap(temp_map, sizeof(SegmentMetadata_t)) != 0)
	{
		ReadMunmapError(errno);
	}
	
	/* now map the whole thing */
	s_seg_info_cache.shared_memory_ptr = mmap(NULL, s_seg_info_cache.seg_sz, PROT_READ | PROT_WRITE, MAP_SHARED, s_seg_info_cache.handle, 0);
	if(s_seg_info_cache.shared_memory_ptr == MAP_FAILED)
	{
		ReadMmapError(errno);
	}
	s_seg_info_cache.is_mapped = TRUE;
#endif
	
	/* tell everyone else that another process is tracking this */
	((SegmentMetadata_t*)s_seg_info_cache.shared_memory_ptr)->procs_tracking += 1;
	
}


static SegmentNode_t* TrackSegment(SegmentList_t* seg_list)
{
	SegmentNode_t* new_seg_node = CreateSegmentNode();
	AddSegmentNode(seg_list, new_seg_node);
	return new_seg_node;
}


static SegmentNode_t* CreateSegmentNode(void)
{
	SegmentNode_t* new_seg_node = mxMalloc(sizeof(SegmentNode_t));
	mexMakeMemoryPersistent(new_seg_node);
	
	new_seg_node->seg_info = s_seg_info_cache;
	new_seg_node->var_node = NULL;
	new_seg_node->prev = NULL;
	new_seg_node->next = NULL;
	
	return new_seg_node;
	
}


static void AddSegmentNode(SegmentList_t* seg_list, SegmentNode_t* seg_node)
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
	
	seg_node->parent_seg_list = seg_list;
	
}


static void RemoveSegmentNode(SegmentList_t* seg_list, SegmentNode_t* seg_node)
{
	
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
	
	mxFree(seg_node);
	
}


static VariableNode_t* TrackVariable(VariableList_t* var_list, SegmentNode_t* seg_node, mxArray* new_var)
{
	VariableNode_t* new_var_node = CreateVariableNode(seg_node, new_var);
	AddVariableNode(var_list, new_var_node);
	return new_var_node;
}


static VariableNode_t* CreateVariableNode(SegmentNode_t* seg_node, mxArray* new_var)
{
	VariableNode_t* new_var_node = mxCalloc(1, sizeof(VariableNode_t));
	mexMakeMemoryPersistent(new_var_node);
	
	new_var_node->seg_node = seg_node;
	new_var_node->var = new_var;
	
	seg_node->var_node = new_var_node;
	
	return new_var_node;
}


static void AddVariableNode(VariableList_t* var_list, VariableNode_t* var_node)
{
	var_node->parent_var_list = var_list;
	
	var_node->next = NULL;
	var_node->prev = var_list->last;
	
	if(var_list->num_vars != 0)
	{
		/* set pointer in the last node to this new node */
		var_list->last->next = var_node;
	}
	else
	{
		/* set the front to this node */
		var_list->first = var_node;
	}
	
	/* append to the end of the list */
	var_list->last = var_node;
	
	/* increment number of variables */
	var_list->num_vars += 1;
	
}


static void RemoveVariableNode(VariableList_t* var_list, VariableNode_t* var_node)
{
	/* reset references in prev and next var node */
	if(var_node->prev != NULL)
	{
		var_node->prev->next = var_node->next;
	}
	
	if(var_node->next != NULL)
	{
		var_node->next->prev = var_node->prev;
	}
	
	if(var_list->first == var_node)
	{
		var_list->first = var_node->next;
	}
	
	if(var_list->last == var_node)
	{
		var_list->last = var_node->prev;
	}
	
	/* decrement number of variables in the list */
	var_list->num_vars -= 1;
}