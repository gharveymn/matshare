#include "headers/mshlists.h"


SegmentInfo_t CreateSegment(size_t seg_sz)
{
	
	SegmentInfo_t seg_info;
	s_SegmentMetadata_t* s_metadata;

#ifdef MSH_WIN
	DWORD hi_sz, lo_sz;
	HANDLE temp_handle;
#else
	handle_t temp_handle;
#endif
	
	/* tracked segments must be up to date to get proper linking */
	UpdateSharedSegments();
	
	if(s_info->num_shared_vars >= SEG_NUM_MAX)
	{
		ReadMexError("TooManyVariablesError", "Could not share the variable. Too many variables are currently being shared.");
	}
	
	/* create a unique new segment */
	msh_segmentnumber_t new_seg_num = s_info->last_seg_num;
	
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
		WriteSegmentName(seg_info.name, new_seg_num);
		SetLastError(0);
		temp_handle = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, hi_sz, lo_sz, seg_info.name);
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
	seg_info.handle = temp_handle;
	seg_info.is_init = TRUE;
	
	seg_info.s_ptr = MapViewOfFile(seg_info.handle, FILE_MAP_ALL_ACCESS, 0, 0, seg_info.seg_sz);
	if(seg_info.s_ptr == NULL)
	{
		ReadMexError("MapDataSegError", "Could not map the data memory segment (Error number %u)", GetLastError());
	}
	seg_info.is_mapped = TRUE;

#else
	
	/* find an available shared memory file name */
	errno = 0;
	do
	{
		/* change the file name */
		new_seg_num = (new_seg_num == SEG_NUM_MAX)? 0 : new_seg_num + 1;
		WriteSegmentName(seg_info.name, new_seg_num);
		temp_handle = shm_open(seg_info.name, O_RDWR | O_CREAT | O_EXCL, s_info->security);
		if(temp_handle == -1)
		{
			if(errno != EEXIST)
			{
				ReadShmOpenError(errno);
			}
		}
	} while(errno == EEXIST);
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
	
	s_metadata = (s_SegmentMetadata_t*)seg_info.s_ptr;
	
	/* set the size of the segment */
	s_metadata->seg_sz = seg_sz;
	
	/* number of processes with variables instantiated using this segment */
	s_metadata->procs_using = 0;
	
	/* number of processes with a handle on this segment */
	s_metadata->procs_tracking = 1;
	
	/* make sure that this segment has been used at least once before freeing it */
	s_metadata->is_used = FALSE;
	
	/* indicates if the segment should be deleted by all processes */
	s_metadata->is_invalid = FALSE;
	
	/* set the segment number */
	s_metadata->seg_num = new_seg_num;
	
	/* refer to nothing since this is the end of the list */
	s_metadata->next_seg_num = -1;
	
	/* check whether to set this as the first segment number */
	if(s_info->num_shared_vars == 0)
	{
		s_info->first_seg_num = new_seg_num;
		
		/* refer to nothing */
		s_metadata->prev_seg_num = -1;
		
	}
	else
	{
		/* set reference in back of list to this segment */
		SegmentMetadata(g_seg_list.last)->next_seg_num = new_seg_num;
		
		/* set reference to previous back of list */
		s_metadata->prev_seg_num = s_info->last_seg_num;
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


SegmentInfo_t OpenSegment(msh_segmentnumber_t seg_num)
{

	
	SegmentInfo_t seg_info;
	s_SegmentMetadata_t* temp_map;
	
	/* update the segment name */
	WriteSegmentName(seg_info.name, seg_num);

#ifdef MSH_WIN
	/* get the new file handle */
	seg_info.handle = OpenFileMapping(FILE_MAP_ALL_ACCESS, TRUE, seg_info.name);
	if(seg_info.handle == NULL)
	{
		ReadMexError("OpenFileError", "Error opening the file mapping (Error Number %u)", GetLastError());
	}
	seg_info.is_init = TRUE;
	
	
	/* map the metadata to get the size of the segment */
	temp_map = MapViewOfFile(seg_info.handle, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(s_SegmentMetadata_t));
	if(temp_map == NULL)
	{
		ReadMexError("MappingError", "Could not fetch the memory segment (Error number: %d).", GetLastError());
	}
	else
	{
		/* get the segment size */
		seg_info.seg_sz = temp_map->seg_sz;
	}
	
	/* unmap the temporary view */
	if(UnmapViewOfFile(temp_map) == 0)
	{
		ReadMexError("UnmapFileError", "Error unmapping the file (Error Number %u)", GetLastError());
	}
	
	/* now map the whole thing */
	seg_info.s_ptr = MapViewOfFile(seg_info.handle, FILE_MAP_ALL_ACCESS, 0, 0, seg_info.seg_sz);
	if(seg_info.s_ptr == NULL)
	{
		ReadMexError("MappingError", "Could not fetch the memory segment (Error number: %d).", GetLastError());
	}
	seg_info.is_mapped = TRUE;

#else
	/* get the new file handle */
	seg_info.handle = shm_open(seg_info.name, O_RDWR, s_info->security);
	if(seg_info.handle == -1)
	{
		ReadShmOpenError(errno);
	}
	seg_info.is_init = TRUE;
	
	/* map the metadata to get the size of the segment */
	temp_map = mmap(NULL, sizeof(s_SegmentMetadata_t), PROT_READ | PROT_WRITE, MAP_SHARED, seg_info.handle, 0);
	if(temp_map == MAP_FAILED)
	{
		ReadMmapError(errno);
	}
	
	/* get the segment size */
	seg_info.seg_sz = temp_map->seg_sz;
	
	/* unmap the temporary map */
	if(munmap(temp_map, sizeof(s_SegmentMetadata_t)) != 0)
	{
		ReadMunmapError(errno);
	}
	
	/* now map the whole thing */
	seg_info.s_ptr = mmap(NULL, seg_info.seg_sz, PROT_READ | PROT_WRITE, MAP_SHARED, seg_info.handle, 0);
	if(seg_info.s_ptr == MAP_FAILED)
	{
		ReadMmapError(errno);
	}
	seg_info.is_mapped = TRUE;
#endif
	
	/* tell everyone else that another process is tracking this */
	((s_SegmentMetadata_t*)seg_info.s_ptr)->procs_tracking += 1;
	
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
	
	return new_seg_node;
	
}


void AddSegmentNode(SegmentList_t* seg_list, SegmentNode_t* seg_node)
{
	if(seg_list != NULL && seg_node != NULL)
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


void CloseSegment(SegmentNode_t* seg_node)
{
#ifdef MSH_UNIX
	bool_t is_last_tracking = FALSE;
#endif
	if(seg_node != NULL)
	{
		
		if(seg_node->seg_info.is_mapped)
		{
			SegmentMetadata(seg_node)->procs_tracking -= 1;
			
			

#ifdef MSH_WIN
			if(UnmapViewOfFile(seg_node->seg_info.s_ptr) == 0)
			{
				ReadMexError("UnmapFileError", "Error unmapping the data file (Error Number %u)", GetLastError());
			}
#else
			/* check if this process will unlink the shared memory */
			is_last_tracking = (bool_t)(SegmentMetadata(seg_node)->procs_tracking == 0);
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
	}
	
}


void DestroySegment(SegmentNode_t* seg_node)
{
#ifdef MSH_UNIX
	bool_t is_last_tracking = FALSE;
#endif
	if(seg_node != NULL)
	{
		
		if(seg_node->seg_info.is_mapped)
		{
			/* signal that this segment is to be freed */
			SegmentMetadata(seg_node)->is_invalid = TRUE;
			
			if(SegmentMetadata(seg_node)->seg_num == s_info->first_seg_num)
			{
				s_info->first_seg_num = SegmentMetadata(seg_node)->next_seg_num;
			}
			
			if(SegmentMetadata(seg_node)->seg_num == s_info->last_seg_num)
			{
				s_info->last_seg_num = SegmentMetadata(seg_node)->prev_seg_num;
			}
			
			if(seg_node->prev != NULL && seg_node->prev->seg_info.is_mapped)
			{
				SegmentMetadata(seg_node->prev)->next_seg_num = SegmentMetadata(seg_node)->next_seg_num;
			}
			
			if(seg_node->next != NULL && seg_node->next->seg_info.is_mapped)
			{
				SegmentMetadata(seg_node->next)->prev_seg_num = SegmentMetadata(seg_node)->prev_seg_num;
			}
			
			SegmentMetadata(seg_node)->procs_tracking -= 1;

#ifdef MSH_WIN
			if(UnmapViewOfFile(seg_node->seg_info.s_ptr) == 0)
			{
				ReadMexError("UnmapFileError", "Error unmapping the data file (Error Number %u)", GetLastError());
			}
#else
			/* check if this process will unlink the shared memory */
			is_last_tracking = (bool_t)(SegmentMetadata(seg_node)->procs_tracking == 0);
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
	}
	
	s_info->num_shared_vars -= 1;
	
}


void CloseSegmentNode(SegmentList_t* seg_list, SegmentNode_t* seg_node)
{
	if(seg_node != NULL)
	{
		
		if(seg_node->var_node != NULL)
		{
			DestroyVariableNode(seg_node->var_node);
		}
		
		CloseSegment(seg_node);
		RemoveSegmentNode(seg_list, seg_node);
		mxFree(seg_node);
	}
}


void DestroySegmentNode(SegmentList_t* seg_list, SegmentNode_t* seg_node)
{
	if(seg_node != NULL)
	{
		
		if(seg_node->var_node != NULL)
		{
			DestroyVariableNode(seg_node->var_node);
		}
		
		if(SegmentMetadata(seg_node)->is_invalid)
		{
			CloseSegment(seg_node);
		}
		else
		{
			DestroySegment(seg_node);
		}
		
		RemoveSegmentNode(seg_list, seg_node);
		mxFree(seg_node);
	}
}


VariableNode_t* CreateVariableNode(SegmentNode_t* seg_node)
{
	VariableNode_t* new_var_node = mxCalloc(1, sizeof(VariableNode_t));
	mexMakeMemoryPersistent(new_var_node);
	
	new_var_node->seg_node = seg_node;
	ShmFetch_(SegmentData(seg_node), &new_var_node->var);
	mexMakeArrayPersistent(new_var_node->var);
	new_var_node->crosslink = &((mxArrayStruct*)new_var_node->var)->CrossLink;
	SegmentMetadata(seg_node)->is_used = TRUE;
	SegmentMetadata(seg_node)->procs_using += 1;
	
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
			SegmentMetadata(var_node->seg_node)->procs_using -= 1;
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
			CloseSegmentNode(seg_list, curr_seg_node);
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
			DestroySegmentNode(seg_list, curr_seg_node);
			curr_seg_node = next_seg_node;
		}
	}
}


void UpdateSharedSegments(void)
{
	
	if(g_info->rev_num == s_info->rev_num)
	{
		/* we should be up to date if this is true */
		return;
	}
	else
	{
		/* make sure that subfunctions don't get stuck in a loop by resetting this immediately */
		g_info->rev_num = s_info->rev_num;
	}
	
	msh_segmentnumber_t curr_seg_num;
	bool_t is_fetched;
	
	SegmentNode_t* curr_seg_node, * next_seg_node, * new_seg_node = NULL;
	SegmentList_t new_seg_list = {NULL, NULL, 0};
	
	curr_seg_node = g_seg_list.first;
	while(curr_seg_node != NULL)
	{
		
		next_seg_node = curr_seg_node->next;
		
		/* Check if the data segment is ready for deletion.
		 * Segment should not be hooked into the shared linked list */
		if(SegmentMetadata(curr_seg_node)->is_invalid)
		{
			CloseSegmentNode(&g_seg_list, curr_seg_node);
		}
		
		curr_seg_node = next_seg_node;
	}
	
	/* construct a new list of segments, reusing what we can */
	for(new_seg_list.num_segs = 0, curr_seg_num = s_info->first_seg_num;
	    new_seg_list.num_segs < s_info->num_shared_vars;
	    new_seg_list.num_segs++, curr_seg_num = SegmentMetadata(new_seg_node)->next_seg_num)
	{
		
		curr_seg_node = g_seg_list.first;
		is_fetched = FALSE;
		while(curr_seg_node != NULL)
		{
			
			/* check if the current segment has been fetched */
			if(SegmentMetadata(curr_seg_node)->seg_num == curr_seg_num)
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
			new_seg_node = CreateSegmentNode(OpenSegment(curr_seg_num));
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
	
	g_seg_list = new_seg_list;
	
}