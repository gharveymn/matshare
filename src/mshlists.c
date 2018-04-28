#include "headers/mshlists.h"

/* only touches segment information in shared memory */
SegmentInfo_t CreateSegment(size_t seg_sz)
{
	
	SegmentInfo_t seg_info;

#ifdef MSH_WIN
	DWORD hi_sz, lo_sz, err = 0;
	HANDLE temp_handle;
#else
	handle_t temp_handle;
	int err = 0;
#endif
	
	/* tracked segments must be up to date to get proper linking */
	UpdateSharedSegments();
	
	/* create a unique new segment */
	seg_num_t new_seg_num = s_info->last_seg_num;
	
	/* set the size */
	seg_info.seg_sz = seg_sz;

#ifdef MSH_WIN
	
	/* split the 64-bit size */
	lo_sz = (DWORD)(seg_info.seg_sz & 0xFFFFFFFFL);
	hi_sz = (DWORD)((seg_info.seg_sz >> 32u) & 0xFFFFFFFFL);
	
	do
	{
		/* change the file name */
		new_seg_num = (new_seg_num == SEG_NUM_MAX)? 0 : new_seg_num + 1;
		err = 0;
		snprintf(seg_info.name, MSH_MAX_NAME_LEN, MSH_SEGMENT_NAME, new_seg_num);
		temp_handle = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, hi_sz, lo_sz, seg_info.name);
		err = GetLastError();
		if(temp_handle == NULL)
		{
			ReleaseProcessLock();
			ReadErrorMex("CreateFileError", "Error creating the file mapping (Error Number %u).", err);
		}
		else if(err == ERROR_ALREADY_EXISTS)
		{
			if(CloseHandle(temp_handle) == 0)
			{
				err = GetLastError();
				ReleaseProcessLock();
				ReadErrorMex("CloseHandleError", "Error closing the file handle (Error Number %u).", err);
			}
		}
	} while(err == ERROR_ALREADY_EXISTS);
	seg_info.handle = temp_handle;
	seg_info.is_init = TRUE;
	
	seg_info.s_ptr = MapViewOfFile(seg_info.handle, FILE_MAP_ALL_ACCESS, 0, 0, seg_info.seg_sz);
	if(seg_info.s_ptr == NULL)
	{
		err = GetLastError();
		ReleaseProcessLock();
		ReadErrorMex("MapDataSegError", "Could not map the data memory segment (Error number %u)", err);
	}
	seg_info.is_mapped = TRUE;

#else
	
	/* find an available shared memory file name */
	do
	{
		/* change the file name */
		new_seg_num = (new_seg_num == SEG_NUM_MAX)? 0 : new_seg_num + 1;
		err = 0;
		snprintf(seg_info.name, MSH_MAX_NAME_LEN, MSH_SEGMENT_NAME, new_seg_num);
		temp_handle = shm_open(seg_info.name, O_RDWR | O_CREAT | O_EXCL, s_info->security);
		if(temp_handle == -1)
		{
			err = errno;
			if(err != EEXIST)
			{
				ReleaseProcessLock();
				ReadShmOpenError(err);
			}
		}
	} while(err == EEXIST);
	seg_info.handle = temp_handle;
	seg_info.is_init = TRUE;
	
	/* change the map size */
	if(ftruncate(seg_info.handle, seg_info.seg_sz) != 0)
	{
		ReleaseProcessLock();
		ReadFtruncateError(errno);
	}
	
	/* map the shared memory */
	seg_info.s_ptr = mmap(NULL, seg_info.seg_sz, PROT_READ | PROT_WRITE, MAP_SHARED, seg_info.handle, 0);
	if(seg_info.s_ptr == MAP_FAILED)
	{
		ReleaseProcessLock();
		ReadMmapError(errno);
	}
	seg_info.is_mapped = TRUE;

#endif
	
	/* set the size of the segment */
	seg_info.s_ptr->seg_sz = seg_sz;
	
	/* number of processes with variables instatiated using this segment */
	seg_info.s_ptr->procs_using = 0;
	
	/* number of processes with a handle on this segment */
	seg_info.s_ptr->procs_tracking = 1;
	
	/* make sure that this segment has been used at least once before freeing it */
	seg_info.s_ptr->is_used = FALSE;
	
	seg_info.s_ptr->is_invalid = FALSE;
	
	/* set the segment number */
	seg_info.s_ptr->seg_num = new_seg_num;
	
	/* refer to nothing since this is the end of the list */
	seg_info.s_ptr->next_seg_num = -1;
	
	/* check whether to set this as the first segment number */
	if(s_info->num_shared_vars == 0)
	{
		s_info->first_seg_num = new_seg_num;
		
		/* refer to nothing */
		seg_info.s_ptr->prev_seg_num = -1;
		
	}
	else
	{
		/* set reference in back of list to this segment */
		g_seg_list.last->seg_info.s_ptr->next_seg_num = new_seg_num;
		
		/* set reference to previous back of list */
		seg_info.s_ptr->prev_seg_num = s_info->last_seg_num;
	}
	
	/* update the last segment number */
	s_info->last_seg_num = new_seg_num;
	
	/* update number of vars in shared memory */
	s_info->num_shared_vars += 1;
	
	/* update the revision number to indicate to retrieve new segments */
	s_info->rev_num += 1;
	g_info->rev_num = s_info->rev_num;
	
	return seg_info;
	
}


SegmentInfo_t OpenSegment(seg_num_t seg_num)
{

#ifdef MSH_WIN
	DWORD err;
#else
	int err;
#endif
	
	SegmentInfo_t seg_info;
	SegmentMetadata_t* temp_map;
	
	/* update the segment name */
	snprintf(seg_info.name, MSH_MAX_NAME_LEN, MSH_SEGMENT_NAME, seg_num);

#ifdef MSH_WIN
	/* get the new file handle */
	seg_info.handle = OpenFileMapping(FILE_MAP_ALL_ACCESS, TRUE, seg_info.name);
	if(seg_info.handle == NULL)
	{
		err = GetLastError();
		ReleaseProcessLock();
		ReadErrorMex("OpenFileError", "Error opening the file mapping (Error Number %u)", err);
	}
	seg_info.is_init = TRUE;
	
	
	/* map the metadata to get the size of the segment */
	temp_map = MapViewOfFile(seg_info.handle, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SegmentMetadata_t));
	if(temp_map == NULL)
	{
		err = GetLastError();
		ReleaseProcessLock();
		ReadErrorMex("MappingError", "Could not fetch the memory segment (Error number: %d).", err);
	}
	else
	{
		/* get the segment size */
		seg_info.seg_sz = temp_map->seg_sz;
	}
	
	/* unmap the temporary view */
	if(UnmapViewOfFile(temp_map) == 0)
	{
		err = GetLastError();
		ReleaseProcessLock();
		ReadErrorMex("UnmapFileError", "Error unmapping the file (Error Number %u)", err);
	}
	
	/* now map the whole thing */
	seg_info.s_ptr = MapViewOfFile(seg_info.handle, FILE_MAP_ALL_ACCESS, 0, 0, seg_info.seg_sz);
	if(seg_info.s_ptr == NULL)
	{
		err = GetLastError();
		ReleaseProcessLock();
		ReadErrorMex("MappingError", "Could not fetch the memory segment (Error number: %d).", err);
	}
	seg_info.is_mapped = TRUE;

#else
	/* get the new file handle */
	seg_info.handle = shm_open(seg_info.name, O_RDWR, s_info->security);
	if(seg_info.handle == -1)
	{
		err = errno;
		ReleaseProcessLock();
		ReadShmOpenError(err);
	}
	seg_info.is_init = TRUE;
	
	/* map the metadata to get the size of the segment */
	temp_map = mmap(NULL, sizeof(SegmentMetadata_t), PROT_READ | PROT_WRITE, MAP_SHARED, seg_info.handle, 0);
	if(temp_map == MAP_FAILED)
	{
		err = errno;
		ReleaseProcessLock();
		ReadMmapError(err);
	}
	
	/* get the segment size */
	seg_info.seg_sz = temp_map->seg_sz;
	
	/* unmap the temporary map */
	if(munmap(temp_map, sizeof(SegmentMetadata_t)) != 0)
	{
		err = errno;
		ReleaseProcessLock();
		ReadMunmapError(err);
	}
	
	/* now map the whole thing */
	seg_info.s_ptr = mmap(NULL, seg_info.seg_sz, PROT_READ | PROT_WRITE, MAP_SHARED, seg_info.handle, 0);
	if(seg_info.s_ptr == MAP_FAILED)
	{
		err = errno;
		ReleaseProcessLock();
		ReadMmapError(err);
	}
	seg_info.is_mapped = TRUE;
#endif
	
	/* tell everyone else that another process is tracking this */
	seg_info.s_ptr->procs_tracking += 1;
	
	return seg_info;
	
}


SegmentNode_t* CreateSegmentNode(SegmentInfo_t seg_info)
{
	SegmentNode_t* new_seg_node = mxMalloc(sizeof(SegmentNode_t));
	mexMakeMemoryPersistent(new_seg_node);
	
	new_seg_node->seg_info = seg_info;
	new_seg_node->var_node = NULL;
	new_seg_node->prev = NULL;
	new_seg_node->next = NULL;
	new_seg_node->parent_seg_list = NULL;
	new_seg_node->will_free = FALSE;
	
	return new_seg_node;
	
}


/* does not touch shared memory */
void AddSegmentNode(SegmentList_t* seg_list, SegmentNode_t* seg_node)
{
	if(seg_list != NULL && seg_node != NULL)
	{
		
		seg_node->parent_seg_list = seg_list;
		
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
		
	}
	
}


void RemoveSegmentNode(SegmentList_t* seg_list, SegmentNode_t* seg_node)
{
	
	if(seg_list != NULL && seg_node != NULL)
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
		
	}
	
}

/* Close the references to this segment. Segment list may not be up to date. */
void CloseSegment(SegmentNode_t* seg_node)
{
	
	bool_t is_last_tracking = FALSE;
	if(seg_node != NULL)
	{
		
		if(seg_node->seg_info.is_mapped)
		{
			seg_node->seg_info.s_ptr->procs_tracking -= 1;
			
			/* check if this process will unlink the shared memory (for linux, but we'll keep the info around for Windows for now) */
			is_last_tracking = (bool_t)(seg_node->seg_info.s_ptr->procs_tracking == 0);

#ifdef MSH_WIN
			if(UnmapViewOfFile(seg_node->seg_info.s_ptr) == 0)
			{
				ReadErrorMex("UnmapFileError", "Error unmapping the data file (Error Number %u)", GetLastError());
			}
#else
			if(munmap(seg_node->seg_info.s_ptr, seg_node->seg_info.seg_sz) != 0)
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
				ReadErrorMex("CloseHandleError", "Error closing the data file handle (Error Number %u)", GetLastError());
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
	}
	
}


/* Signal to destroy this segment. Segment list must be fully up to date beforehand */
void DestroySegment(SegmentNode_t* seg_node)
{
	bool_t is_last_tracking = FALSE;
	if(seg_node != NULL)
	{
		
		if(seg_node->seg_info.is_mapped)
		{
			/* signal that this segment is to be freed */
			seg_node->seg_info.s_ptr->is_invalid = TRUE;
			
			if(seg_node->seg_info.s_ptr->seg_num == s_info->first_seg_num)
			{
				s_info->first_seg_num = seg_node->seg_info.s_ptr->next_seg_num;
			}
			
			if(seg_node->seg_info.s_ptr->seg_num == s_info->last_seg_num)
			{
				s_info->last_seg_num = seg_node->seg_info.s_ptr->prev_seg_num;
			}
			
			if(seg_node->prev != NULL && seg_node->prev->seg_info.is_mapped)
			{
				seg_node->prev->seg_info.s_ptr->next_seg_num = seg_node->seg_info.s_ptr->next_seg_num;
			}
			
			if(seg_node->next != NULL && seg_node->next->seg_info.is_mapped)
			{
				seg_node->next->seg_info.s_ptr->prev_seg_num = seg_node->seg_info.s_ptr->prev_seg_num;
			}
			
			seg_node->seg_info.s_ptr->procs_tracking -= 1;
			
			/* check if this process will unlink the shared memory (for linux, but we'll keep the info around for Windows for now) */
			is_last_tracking = (bool_t)(seg_node->seg_info.s_ptr->procs_tracking == 0);

#ifdef MSH_WIN
			if(UnmapViewOfFile(seg_node->seg_info.s_ptr) == 0)
			{
				ReadErrorMex("UnmapFileError", "Error unmapping the data file (Error Number %u)", GetLastError());
			}
#else
			if(munmap(seg_node->seg_info.s_ptr, seg_node->seg_info.seg_sz) != 0)
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
				ReadErrorMex("CloseHandleError", "Error closing the data file handle (Error Number %u)", GetLastError());
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
	}
	
	s_info->num_shared_vars -= 1;
	
}

void CloseSegmentNode(SegmentNode_t* seg_node)
{
	if(seg_node != NULL)
	{
		
		if(seg_node->var_node != NULL)
		{
			DestroyVariableNode(seg_node->var_node);
		}
		
		CloseSegment(seg_node);
		RemoveSegmentNode(seg_node->parent_seg_list, seg_node);
		mxFree(seg_node);
	}
}


void DestroySegmentNode(SegmentNode_t* seg_node)
{
	if(seg_node != NULL)
	{
		
		if(seg_node->var_node != NULL)
		{
			DestroyVariableNode(seg_node->var_node);
		}
		
		if(seg_node->seg_info.s_ptr->is_invalid)
		{
			CloseSegment(seg_node);
		}
		else
		{
			DestroySegment(seg_node);
		}
		
		RemoveSegmentNode(seg_node->parent_seg_list, seg_node);
		mxFree(seg_node);
	}
}


VariableNode_t* CreateVariableNode(SegmentNode_t* seg_node)
{
	VariableNode_t* new_var_node = mxCalloc(1, sizeof(VariableNode_t));
	mexMakeMemoryPersistent(new_var_node);
	
	new_var_node->seg_node = seg_node;
	ShmFetch((byte_t*)seg_node->seg_info.s_ptr, &new_var_node->var);
	new_var_node->crosslink = &((mxArrayStruct*)new_var_node->var)->CrossLink;
	seg_node->seg_info.s_ptr->is_used = TRUE;
	seg_node->seg_info.s_ptr->procs_using += 1;
	
	seg_node->var_node = new_var_node;
	
	return new_var_node;
}


void AddVariableNode(VariableList_t* var_list, VariableNode_t* var_node)
{
	if(var_list != NULL && var_node != NULL)
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
	
}


/* only remove the node from the list, does not destroy */
void RemoveVariableNode(VariableList_t* var_list, VariableNode_t* var_node)
{
	
	if(var_list != NULL && var_node != NULL)
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
}


void DestroyVariable(VariableNode_t* var_node)
{
	if(var_node != NULL)
	{
		/* NULL all of the Matlab pointers */
		ShmDetach(var_node->var);
		mxDestroyArray(var_node->var);
		
		/* this should block DestroyVariableNode from running twice */
		if(var_node->seg_node != NULL)
		{
			var_node->seg_node->var_node = NULL;
			
			/* decrement number of processes tracking this variable */
			var_node->seg_node->seg_info.s_ptr->procs_using -= 1;
		}
	}
}

void DestroyVariableNode(VariableNode_t* var_node)
{
	if(var_node != NULL)
	{
		DestroyVariable(var_node);
		RemoveVariableNode(var_node->parent_var_list, var_node);
		mxFree(var_node);
	}
}

void CleanVariableList(VariableList_t* var_list)
{
	VariableNode_t* curr_var_node, * next_var_node;
	if(var_list != NULL)
	{
		curr_var_node = var_list->first;
		while(curr_var_node != NULL)
		{
			next_var_node = curr_var_node->next;
			DestroyVariableNode(curr_var_node);
			curr_var_node = next_var_node;
		}
	}
}


void CleanSegmentList(SegmentList_t* seg_list)
{
	SegmentNode_t* curr_seg_node, * next_seg_node;
	if(seg_list != NULL)
	{
		curr_seg_node = seg_list->first;
		while(curr_seg_node != NULL)
		{
			next_seg_node = curr_seg_node->next;
			CloseSegmentNode(curr_seg_node);
			curr_seg_node = next_seg_node;
		}
	}
}


void DestroySegmentList(SegmentList_t* seg_list)
{
	SegmentNode_t* curr_seg_node, * next_seg_node;
	if(seg_list != NULL)
	{
		curr_seg_node = seg_list->first;
		while(curr_seg_node != NULL)
		{
			next_seg_node = curr_seg_node->next;
			DestroySegmentNode(curr_seg_node);
			curr_seg_node = next_seg_node;
		}
	}
}